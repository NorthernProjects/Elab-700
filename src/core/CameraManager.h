#pragma once

#include <QFutureWatcher>
#include <QObject>
#include <QScopedPointer>
#include <QTimer>

#include "CameraBackend.h"

// Owns the active CameraBackend, periodically re-scans for devices when
// closed, and opens the first device it finds automatically (V1 requirement:
// "detect the connected camera" — a single-camera classroom setup needs no
// device picker on the student screen; the teacher panel can still show the
// detected model/id).
class CameraManager : public QObject {
    Q_OBJECT

public:
    explicit CameraManager(QObject *parent = nullptr);

    CameraBackend *backend() const { return m_backend.data(); }
    bool isConnected() const;
    QVector<CameraDeviceInfo> lastKnownDevices() const { return m_devices; }
    QString currentDeviceId() const { return m_currentDeviceId; }

public slots:
    void rescan();

    // Manually connect to a specific device from lastKnownDevices(),
    // bypassing the "looks like the microscope" auto-connect threshold —
    // for the teacher/student to override when automatic detection picked
    // the wrong camera (or none at all). Closes whatever is currently open
    // first if it's a different device.
    bool forceConnect(const QString &deviceId);

    // Gracefully closes the active camera in software — an explicit
    // alternative to physically unplugging it (or leaving it plugged in but
    // idle/offline), both of which are harder on the hardware than just
    // telling the driver to stop.
    void disconnectCamera();

signals:
    void connected(const CameraDeviceInfo &info);
    void disconnected();

private slots:
    void onProbeFinished();

private:
    void connectToBestCandidate();

    QScopedPointer<CameraBackend> m_backend;
    QTimer m_rescanTimer;
    QVector<CameraDeviceInfo> m_devices;
    QString m_currentDeviceId;
    // Set by disconnectCamera(), cleared by forceConnect(): otherwise the
    // periodic rescan would auto-reconnect the microscope again within a
    // couple of seconds, defeating the point of an explicit disconnect.
    bool m_manuallyDisconnected = false;

    // Probing several capture indices is slow (driver-dependent, can be a
    // full second or more) — run it on a worker thread (see
    // UvcCameraBackend::probeDevices()) instead of blocking the UI thread
    // every 2s while no camera is connected, which used to freeze the whole
    // app (including the startup splash animation) for the duration of each
    // scan.
    QFutureWatcher<QVector<CameraDeviceInfo>> m_probeWatcher;
    bool m_probeInFlight = false;
};
