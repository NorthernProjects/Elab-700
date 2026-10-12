#pragma once

#include <atomic>
#include <memory>

#include <QElapsedTimer>
#include <QMutex>
#include <QSemaphore>
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
    bool isOpening() const override { return m_opening; }

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

    // The capture thread does everything with the camera: it opens it,
    // reads frames, and finally destroys it — DirectShow/COM objects must
    // live and die on one thread, and doing the ~1 s open there keeps the
    // GUI responsive. The GUI thread only ever receives the newest finished
    // frame (deliverLatestFrame), so a slow UI drops frames instead of
    // building up a backlog.
    //
    // async = true returns at once; the outcome arrives as deviceConnected()
    // or openFailed(). async = false waits for the open attempt to finish
    // and returns its outcome (used for resolution changes, whose callers
    // read the new resolution right afterwards).
    bool startCapture(int index, const QString &deviceId, const QSize &size, bool async);
    void stopWorker();
    void captureLoop(int generation, int index, QString deviceId, QSize size, bool async);
    void deliverLatestFrame();
    void onOpened(int generation, const QString &deviceId, int index);
    void onOpenFailed(int generation, const QString &deviceId);
    void onStreamLost(int generation);

    // Created, used and destroyed by the capture thread only; the mutex
    // serializes that thread's reads against the GUI thread's property
    // get/set calls (cv::VideoCapture is not thread-safe). May be null.
    std::unique_ptr<cv::VideoCapture> m_capture;
    mutable QMutex m_captureMutex;
    int m_openIndex = -1;
    QString m_openDeviceId;
    mutable std::unique_ptr<DirectShowControls> m_realControls;

    QThread *m_thread = nullptr;
    std::atomic<bool> m_stopRequested{false};
    std::atomic<bool> m_opened{false};  // camera is open and streaming
    std::atomic<bool> m_opening{false}; // open attempt in progress
    QSemaphore m_openAttemptDone;       // released when an open attempt finished
    bool m_syncOpenResult = false;      // outcome handed to a synchronous waiter
    bool m_announcedConnected = false;  // deviceConnected() was emitted (GUI thread only)
    int m_generation = 0; // GUI thread only; ignores stale events from a previous thread

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
