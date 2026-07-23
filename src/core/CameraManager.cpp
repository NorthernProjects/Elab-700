#include "CameraManager.h"

#include <QTimer>
#include <QtConcurrent/QtConcurrentRun>

#include "UvcCameraBackend.h"

namespace {
// The OMAX 83S sensor negotiates ~2592x1944 (5MP, area ~5.0M); a laptop's
// built-in webcam tops out around 1920x1080 (2MP, area ~2.1M) even on
// higher-end machines. This floor sits comfortably between the two so
// auto-connect can never silently pick the laptop's own camera just
// because it's the only thing detected — it shows "no camera detected"
// instead and waits for a manual pick (see forceConnect) or the real
// microscope to appear.
constexpr long kMinMicroscopeArea = 3000000;
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

void CameraManager::rescan()
{
    if (m_backend->isOpen() || m_probeInFlight)
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
    // Auto-connect only to whichever candidate looks like the microscope's
    // high-res sensor, picking the largest if several qualify — never the
    // laptop's own webcam just because it's the only thing present (see
    // kMinMicroscopeArea). Anything that doesn't qualify is still listed in
    // m_devices for manual selection (see forceConnect), just not opened
    // automatically.
    const CameraDeviceInfo *best = nullptr;
    long bestArea = 0;
    for (const CameraDeviceInfo &device : m_devices) {
        const long area = static_cast<long>(device.resolution.width()) * device.resolution.height();
        if (area >= kMinMicroscopeArea && area > bestArea) {
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
    m_manuallyDisconnected = false;

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
}
