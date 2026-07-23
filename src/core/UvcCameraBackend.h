#pragma once

#include <QElapsedTimer>
#include <QScopedPointer>
#include <QTimer>

#include "CameraBackend.h"

namespace cv {
class VideoCapture;
}

// Real camera backend using OpenCV's platform capture API: DirectShow on
// Windows (cv::CAP_DSHOW), AVFoundation on macOS (cv::CAP_AVFOUNDATION) —
// picked at compile time in the .cpp, everything else below is identical
// between the two.
//
// The microscope camera (OMAX/ToupTek SCMOS05000KPA) ships with a ToupCam
// SDK, but on Windows this hardware/driver combination has the SDK's own
// device enumeration (Toupcam_EnumV2) reliably find nothing even though the
// camera works perfectly — the OS exposes it through the standard UVC
// (USB Video Class) camera interface instead (same class a laptop's built-in
// webcam uses), so it's really just a regular UVC webcam as far as the OS is
// concerned. This backend talks to it exactly like any UVC webcam via
// OpenCV, which is simpler and more reliable here than guessing at the
// proprietary SDK's ABI, and the same reasoning carries over to macOS
// unchanged (AVFoundation exposes UVC devices the same way). See README.md.
class UvcCameraBackend : public CameraBackend {
    Q_OBJECT

public:
    explicit UvcCameraBackend(QObject *parent = nullptr);
    ~UvcCameraBackend() override;

    QString backendName() const override;
    QVector<CameraDeviceInfo> enumerateDevices() override;

    // The actual probing loop, factored out as a free function that touches
    // no backend state (each candidate is a throwaway local VideoCapture) —
    // safe to run on a worker thread via QtConcurrent so probing several
    // capture indices (slow, especially on some drivers/machines) never
    // blocks the UI thread. CameraManager::rescan() calls this instead of
    // enumerateDevices() precisely to get that off-thread behavior.
    static QVector<CameraDeviceInfo> probeDevices();

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

private slots:
    void captureFrame();

private:
    QScopedPointer<cv::VideoCapture> m_capture;
    QTimer m_captureTimer;
    QElapsedTimer m_fpsClock;
    int m_frameCountSinceFpsUpdate = 0;
    int m_openIndex = -1;

    // A UVC driver commonly drops a handful of reads while it renegotiates
    // the stream right after a resolution change (see setResolution()) —
    // that's normal settling, not a real disconnect. Only declare the stream
    // actually lost after it stays silent for a sustained stretch, tracked
    // by wall-clock time rather than a frame count so it scales with
    // whatever fps the current resolution allows.
    QElapsedTimer m_lastGoodFrameClock;

    // One-shot software white balance: the platform AUTO_WB property is
    // unreliable across drivers, so "Auto" instead measures the next
    // captured frame's per-channel averages (point the camera at a blank/
    // bright area when clicking Auto) and applies fixed correction gains to
    // every frame after that, until recalibrated.
    bool m_wbCalibrationRequested = false;
    double m_wbGainB = 1.0;
    double m_wbGainG = 1.0;
    double m_wbGainR = 1.0;
};
