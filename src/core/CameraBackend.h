#pragma once

#include <QObject>
#include <QVector>

#include "CameraTypes.h"

// Abstract camera source. The UI and CaptureManager only ever talk to this
// interface, never to a concrete backend, so a new hardware path (a generic
// UVC webcam, an HDMI capture card, a different vendor SDK) can be added by
// implementing this class without touching the UI.
//
// Backends must never invent frames: if no real device is connected/opened,
// they must stay silent (no frameReady emissions) and report isOpen() ==
// false. The UI is responsible for showing a clear "no camera" message in
// that case instead of the video view.
class CameraBackend : public QObject {
    Q_OBJECT

public:
    explicit CameraBackend(QObject *parent = nullptr) : QObject(parent) {}
    ~CameraBackend() override = default;

    // Human-readable name of this backend, e.g. "ToupCam SDK", "No camera".
    virtual QString backendName() const = 0;

    // Re-scans for connected devices. Safe to call while closed.
    virtual QVector<CameraDeviceInfo> enumerateDevices() = 0;

    virtual bool open(const QString &deviceId) = 0;
    virtual void close() = 0;
    virtual bool isOpen() const = 0;

    virtual QSize currentResolution() const = 0;
    virtual QVector<QSize> supportedResolutions() const = 0;
    virtual bool setResolution(const QSize &size) = 0;

    virtual bool setAutoWhiteBalance(bool enabled) = 0;
    virtual bool autoWhiteBalance() const = 0;

    virtual bool setAutoExposure(bool enabled) = 0;
    virtual bool autoExposure() const = 0;

    // Normalized 0-100 controls; backend maps to native units internally.
    virtual bool setBrightness(int value0to100) = 0;
    virtual int brightness() const = 0;

    virtual bool setExposure(int value0to100) = 0; // manual mode only
    virtual int exposure() const = 0;

    virtual bool setGain(int value0to100) = 0;
    virtual int gain() const = 0;

signals:
    void frameReady(const CameraFrame &frame);
    void deviceConnected(const CameraDeviceInfo &info);
    void deviceDisconnected();
    void errorOccurred(const QString &message);
    void fpsUpdated(double fps);
};
