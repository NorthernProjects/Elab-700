#pragma once

#include "CameraBackend.h"

// Backend used whenever the ToupCam SDK is not available at compile time.
// Never fabricates a device or a frame: enumerateDevices() always returns
// empty and open() always fails, so the UI honestly reports "no camera".
class NullCameraBackend : public CameraBackend {
    Q_OBJECT

public:
    explicit NullCameraBackend(QObject *parent = nullptr);

    QString backendName() const override;
    QVector<CameraDeviceInfo> enumerateDevices() override;

    bool open(const QString &deviceId) override;
    void close() override;
    bool isOpen() const override;

    QSize currentResolution() const override;
    QVector<QSize> supportedResolutions() const override;
    bool setResolution(const QSize &size) override;

    bool setAutoWhiteBalance(bool enabled) override;
    bool autoWhiteBalance() const override;

    bool setAutoExposure(bool enabled) override;
    bool autoExposure() const override;

    bool setBrightness(int value0to100) override;
    int brightness() const override;

    bool setExposure(int value0to100) override;
    int exposure() const override;

    bool setGain(int value0to100) override;
    int gain() const override;
};
