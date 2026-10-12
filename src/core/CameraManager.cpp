#include "CameraManager.h"

#include <limits>

#include <QStringList>
#include <QTimer>
#include <QtConcurrent/QtConcurrentRun>

#include "DirectShowInfo.h"
#include "UvcCameraBackend.h"

namespace {
// The OMAX 83S can negotiate as high as ~2592x1944 (5MP) but in practice
// often reports a much lower resolution during the quick probe (real-world
// testing found it landing on 640x480/VGA, area ~307k) — driver/timing
// dependent, not something this app controls. The floor is set just under
// that, low enough to still recognize it reliably. This does mean a
// laptop's own low-res webcam could also pass on a machine that has one;
// this app's primary classroom setup (a desktop with only the microscope's
// camera attached) doesn't hit that case, and the device picker still lets
// a teacher manually override if it ever does.
constexpr long kMinMicroscopeArea = 300000;

// A camera that was seen but could not be opened (busy in another program…)
// is retried this often — slowly, to leave it alone.
constexpr qint64 kRetryProbeMs = 15000;

bool looksLikeMicroscope(const CameraDeviceInfo &device)
{
    const long area = static_cast<long>(device.resolution.width()) * device.resolution.height();
    return area >= kMinMicroscopeArea;
}

bool containsAny(const QString &text, const QStringList &keywords)
{
    for (const QString &keyword : keywords) {
        if (text.contains(keyword, Qt::CaseInsensitive))
            return true;
    }
    return false;
}

bool nameLooksLikeMicroscope(const QString &name)
{
    static const QStringList keywords = {
        QStringLiteral("omax"), QStringLiteral("microscop"), QStringLiteral("toupcam"),
        QStringLiteral("touptek"), QStringLiteral("scmos"), QStringLiteral("biolam")};
    return containsAny(name, keywords);
}

bool nameLooksLikeWebcam(const QString &name)
{
    static const QStringList keywords = {
        QStringLiteral("integrated"), QStringLiteral("intégr"), QStringLiteral("webcam"),
        QStringLiteral("facetime"), QStringLiteral("hd camera"), QStringLiteral("ir camera"),
        QStringLiteral("virtual"), QStringLiteral("droidcam"), QStringLiteral("iriun"),
        QStringLiteral("camo")};
    return containsAny(name, keywords);
}

// The microscope's native presets (640x480, 1280x960, 2592x1944) are all
// 4:3; a laptop's built-in webcam is typically 16:9 (e.g. 1280x720).
bool isFourByThree(const QSize &size)
{
    if (size.height() <= 0)
        return false;
    const double ratio = static_cast<double>(size.width()) / size.height();
    return qAbs(ratio - 4.0 / 3.0) < 0.05;
}

// Decides which detected camera is the microscope. The driver's own name is
// the reliable signal — if any camera is named like a microscope (OMAX, …),
// exactly those win. Without that (generic driver names like "USB2.0 PC
// CAMERA", or a non-Windows system) fall back to the image shape: 4:3 and
// not named like a laptop webcam.
void classifyDevices(QVector<CameraDeviceInfo> &devices)
{
    bool anyNamedMicroscope = false;
    for (const CameraDeviceInfo &device : devices)
        anyNamedMicroscope = anyNamedMicroscope || nameLooksLikeMicroscope(device.deviceName);

    for (CameraDeviceInfo &device : devices) {
        device.likelyMicroscope = anyNamedMicroscope
            ? nameLooksLikeMicroscope(device.deviceName)
            : (isFourByThree(device.resolution) && !nameLooksLikeWebcam(device.deviceName));
    }
}
}

CameraManager::CameraManager(QObject *parent) : QObject(parent)
{
    m_backend.reset(new UvcCameraBackend(this));

    connect(m_backend.data(), &CameraBackend::deviceConnected, this, &CameraManager::connected);
    connect(m_backend.data(), &CameraBackend::deviceDisconnected, this, &CameraManager::disconnected);
    connect(m_backend.data(), &CameraBackend::openFailed, this, &CameraManager::connectFailed);
    // Any lost/switched connection deserves a fresh full probe right away.
    connect(m_backend.data(), &CameraBackend::deviceDisconnected, this, [this]() { m_forceProbe = true; });

    connect(&m_rescanTimer, &QTimer::timeout, this, &CameraManager::rescan);
    m_rescanTimer.setInterval(2000);
    m_rescanTimer.start();

    connect(&m_probeWatcher, &QFutureWatcherBase::finished, this, &CameraManager::onProbeFinished);

    // Deferred: CameraManager is constructed as a MainWindow member before
    // MainWindow's own constructor body runs, so calling rescan() here
    // directly could open the camera (and emit connected()) before
    // MainWindow has had a chance to connect to that signal, silently
    // dropping the very first notification.
    QTimer::singleShot(0, this, &CameraManager::rescan);
}

bool CameraManager::isConnected() const
{
    return m_backend && m_backend->isOpen();
}

QVector<CameraDeviceInfo> CameraManager::microscopeDevices() const
{
    QVector<CameraDeviceInfo> result;
    for (const CameraDeviceInfo &device : m_devices) {
        if (looksLikeMicroscope(device) || device.likelyMicroscope)
            result.append(device);
    }
    return result;
}

void CameraManager::rescan()
{
    // m_manuallyDisconnected (= powered off) must stop this cold: probing
    // briefly opens every candidate device index (see probeDevices()), so
    // without this check the "off" camera was still getting opened/closed
    // every 2 seconds by the periodic rescan timer even while switched off
    // — not the real, hands-off "leave the camera alone" state a power
    // button should provide.
    if (m_backend->isOpen() || m_backend->isOpening() || m_probeInFlight || m_manuallyDisconnected)
        return;

    // Probing runs entirely on a worker thread so a slow/flaky driver never
    // freezes the UI — onProbeFinished() picks up the result back on this
    // (the UI) thread.
    m_probeInFlight = true;
    const QString lastSignature = m_lastSignature;
    const bool force = m_forceProbe;
    const qint64 sinceLastProbe = m_lastProbeClock.isValid() ? m_lastProbeClock.elapsed() : kRetryProbeMs + 1;
    m_probeWatcher.setFuture(QtConcurrent::run([lastSignature, force, sinceLastProbe]() {
        ProbeResult result;
        result.signature = directShowDeviceSignature();
        const bool listChanged = result.signature.isEmpty() || result.signature != lastSignature;
        result.probed = force || listChanged || sinceLastProbe > kRetryProbeMs;
        if (result.probed)
            result.devices = UvcCameraBackend::probeDevices();
        return result;
    }));
}

void CameraManager::onProbeFinished()
{
    m_probeInFlight = false;
    const ProbeResult result = m_probeWatcher.result();
    if (!result.probed)
        return; // nothing changed, nothing was touched

    m_devices = result.devices;
    classifyDevices(m_devices);
    m_lastSignature = result.signature;
    m_forceProbe = false;
    m_lastProbeClock.restart();

    // The camera may have been opened manually (forceConnect) or explicitly
    // disconnected while this scan was in flight — either way, the result we
    // just got is stale for the purpose of auto-connecting.
    if (m_backend->isOpen() || m_manuallyDisconnected)
        return;

    connectToBestCandidate();
}

void CameraManager::connectToBestCandidate()
{
    // First choice: the camera classified as the microscope (see
    // classifyDevices — by driver name when possible, else by 4:3 shape),
    // the smallest if several qualify. Never the laptop's own webcam just
    // because it's also there. Smallest rather than biggest on purpose: the
    // OMAX negotiates a small default probe resolution (640x480) while a
    // laptop webcam commonly reports something larger (e.g. 1280x720).
    //
    // Fallback when nothing is classified as the microscope: any camera
    // above the kMinMicroscopeArea floor (smallest first), so a teacher with
    // only a plain webcam still gets a picture. Everything else stays
    // listed in m_devices for manual selection (see forceConnect).
    const CameraDeviceInfo *best = nullptr;
    long bestArea = std::numeric_limits<long>::max();
    for (const bool classifiedOnly : {true, false}) {
        for (const CameraDeviceInfo &device : m_devices) {
            const long area = static_cast<long>(device.resolution.width()) * device.resolution.height();
            const bool eligible = classifiedOnly ? device.likelyMicroscope : looksLikeMicroscope(device);
            if (eligible && area < bestArea) {
                bestArea = area;
                best = &device;
            }
        }
        if (best)
            break;
    }

    if (best) {
        m_currentDeviceId = best->id;
        if (m_backend->open(best->id))
            emit connecting();
    }
}

bool CameraManager::forceConnect(const QString &deviceId)
{
    if (m_manuallyDisconnected) {
        m_manuallyDisconnected = false;
        emit poweredOnChanged(true);
    }

    if (m_backend->isOpening())
        return false; // let the attempt in progress finish first

    if (m_backend->isOpen()) {
        if (m_currentDeviceId == deviceId)
            return true;
        m_backend->close();
    }

    if (m_backend->open(deviceId)) {
        m_currentDeviceId = deviceId;
        emit connecting();
        return true;
    }
    return false;
}

void CameraManager::disconnectCamera()
{
    if (!m_backend->isOpen() && !m_backend->isOpening())
        return;
    m_manuallyDisconnected = true;
    m_backend->close();
    emit poweredOnChanged(false);
}

void CameraManager::setPoweredOn(bool on)
{
    if (on == isPoweredOn())
        return;

    if (!on) {
        // Not just disconnectCamera(): that function no-ops when nothing is
        // currently open (e.g. "Aucune caméra détectée"), which would leave
        // the power button stuck showing "on" with nothing to actually turn
        // off. Set the flag and emit unconditionally; only close the backend
        // if there's actually something open to close.
        m_manuallyDisconnected = true;
        if (m_backend->isOpen() || m_backend->isOpening())
            m_backend->close();
        emit poweredOnChanged(false);
        return;
    }

    m_manuallyDisconnected = false;
    m_forceProbe = true;
    emit poweredOnChanged(true);
    rescan(); // no-op if already open/probing; otherwise retries right away
}
