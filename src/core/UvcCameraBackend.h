#pragma once

#include <atomic>
#include <memory>

#include <QElapsedTimer>
#include <QMutex>
#include <QScopedPointer>

#include "CameraBackend.h"

namespace cv {
class VideoCapture;
}

class DirectShowControls;
class QThread;

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

    // How many full probes (which briefly open every camera) ran so far —
    // diagnostics/tests only.
    static int probeCount();

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

    bool setExposure(int value0to100) override;
    int exposure() const override;

    bool setGain(int value0to100) override;
    int gain() const override;

private:
    // The camera's real exposure/gain controls (see DirectShowControls),
    // bound lazily on first use and dropped on close(); null when the
    // platform/driver doesn't offer them, in which case the OpenCV property
    // guesses are used instead.
    DirectShowControls *realControls() const;
    void applyDefaultGain();

    // Shared by open() and setResolution(): creates and configures a fresh
    // cv::VideoCapture at the given index/size, replacing m_capture on
    // success. Reopening (rather than calling set() on an already-streaming
    // capture) is what actually gets a DirectShow driver to renegotiate the
    // frame size reliably — many drivers silently ignore a mid-stream
    // FRAME_WIDTH/HEIGHT change instead.
    bool openCaptureAt(int index, const QSize &size);

    // Frames are read on a dedicated thread (captureLoop): a blocking
    // cv::VideoCapture::read() on the GUI thread used to freeze the whole
    // interface for up to a frame interval, every frame. The GUI thread only
    // ever receives the newest finished frame (deliverLatestFrame), so a slow
    // UI drops frames instead of building up a backlog.
    void startWorker();
    void stopWorker();
    void captureLoop(int generation);
    void deliverLatestFrame();
    void onStreamLost(int generation);

    QScopedPointer<cv::VideoCapture> m_capture;
    // Held by the worker for the duration of each read, and by the GUI thread
    // around every direct use of m_capture (property get/set), since
    // cv::VideoCapture is not thread-safe.
    mutable QMutex m_captureMutex;
    int m_openIndex = -1;
    mutable std::unique_ptr<DirectShowControls> m_realControls;

    QThread *m_thread = nullptr;
    std::atomic<bool> m_stopRequested{false};
    int m_generation = 0; // GUI thread only; ignores stale events from a previous worker

    QMutex m_frameMutex;
    CameraFrame m_latestFrame;
    bool m_deliveryPending = false;

    // One-shot software white balance: the platform AUTO_WB property is
    // unreliable across drivers, so "Auto" instead measures the next
    // captured frame's per-channel averages (point the camera at a blank/
    // bright area when clicking Auto) and applies fixed correction gains to
    // every frame after that, until recalibrated.
    QMutex m_wbMutex;
    bool m_wbCalibrationRequested = false;
    double m_wbGainB = 1.0;
    double m_wbGainG = 1.0;
    double m_wbGainR = 1.0;
};
