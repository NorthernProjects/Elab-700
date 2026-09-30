#include "CameraManager.h"

#include <limits>

#include <QTimer>
#include <QtConcurrent/QtConcurrentRun>

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

bool looksLikeMicroscope(const CameraDeviceInfo &device)
{
    const long area = static_cast<long>(device.resolution.width()) * device.resolution.height();
    return area >= kMinMicroscopeArea;
}
}

CameraManager::CameraManager(QObject *parent) : QObject(parent)
{
    m_backend.reset(new UvcCameraBackend(this));

    connect(m_backend.data(), &CameraBackend::deviceConnected, this, &CameraManager::connected);
    connect(m_backend.data(), &CameraBackend::deviceDisconnected, this, &CameraManager::disconnected);

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
        if (looksLikeMicroscope(device))
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
    if (m_backend->isOpen() || m_probeInFlight || m_manuallyDisconnected)
        return;

    // Probing several capture indices runs entirely on a worker thread
    // (see probeDevices()) so a slow/flaky driver never freezes the UI —
    // onProbeFinished() picks up the result back on this (the UI) thread.
    m_probeInFlight = true;
    m_probeWatcher.setFuture(QtConcurrent::run(&UvcCameraBackend::probeDevices));
}

void CameraManager::onProbeFinished()
{
    m_probeInFlight = false;
    m_devices = m_probeWatcher.result();

    // The camera may have been opened manually (forceConnect) or explicitly
    // disconnected while this scan was in flight — either way, the result we
    // just got is stale for the purpose of auto-connecting.
    if (m_backend->isOpen() || m_manuallyDisconnected)
        return;

    connectToBestCandidate();
}

void CameraManager::connectToBestCandidate()
{
    // Auto-connect only to whichever candidate looks like the microscope,
    // picking the SMALLEST qualifying resolution if several are present —
    // never the laptop's own webcam just because it's also there (see
    // kMinMicroscopeArea for the lower bound that excludes tiny/bogus
    // readings). This is deliberately the opposite of "pick the biggest
    // sensor": in practice the OMAX camera negotiates a small default probe
    // resolution (640x480) while a laptop's built-in webcam commonly
    // reports something larger (e.g. 1280x720) — picking the smallest
    // qualifying candidate is what actually lands on the microscope on
    // real hardware. Anything that doesn't qualify is still listed in
    // m_devices for manual selection (see forceConnect), just not opened
    // automatically.
    const CameraDeviceInfo *best = nullptr;
    long bestArea = std::numeric_limits<long>::max();
    for (const CameraDeviceInfo &device : m_devices) {
        const long area = static_cast<long>(device.resolution.width()) * device.resolution.height();
        if (looksLikeMicroscope(device) && area < bestArea) {
            bestArea = area;
            best = &device;
        }
    }

    if (best) {
        m_currentDeviceId = best->id;
        m_backend->open(best->id);
    }
}

bool CameraManager::forceConnect(const QString &deviceId)
{
    if (m_manuallyDisconnected) {
        m_manuallyDisconnected = false;
        emit poweredOnChanged(true);
    }

    if (m_backend->isOpen()) {
        if (m_currentDeviceId == deviceId)
            return true;
        m_backend->close();
    }

    if (m_backend->open(deviceId)) {
        m_currentDeviceId = deviceId;
        return true;
    }
    return false;
}

void CameraManager::disconnectCamera()
{
    if (!m_backend->isOpen())
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
        if (m_backend->isOpen())
            m_backend->close();
        emit poweredOnChanged(false);
        return;
    }

    m_manuallyDisconnected = false;
    emit poweredOnChanged(true);
    rescan(); // no-op if already open/probing; otherwise retries right away
}
