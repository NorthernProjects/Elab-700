#include "UvcCameraBackend.h"

#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

#include <vector>

#include <QImage>

namespace {
constexpr int kMaxProbedIndex = 8;
constexpr int kCaptureIntervalMs = 33; // ~30 fps target

// The one platform difference: which OpenCV capture API backs cv::VideoCapture.
#if defined(Q_OS_WIN)
constexpr int kCaptureBackend = cv::CAP_DSHOW;
#elif defined(Q_OS_MAC)
constexpr int kCaptureBackend = cv::CAP_AVFOUNDATION;
#else
constexpr int kCaptureBackend = cv::CAP_ANY;
#endif
}

UvcCameraBackend::UvcCameraBackend(QObject *parent) : CameraBackend(parent)
{
    connect(&m_captureTimer, &QTimer::timeout, this, &UvcCameraBackend::captureFrame);
}

UvcCameraBackend::~UvcCameraBackend()
{
    close();
}

QString UvcCameraBackend::backendName() const
{
#if defined(Q_OS_WIN)
    return QStringLiteral("Caméra USB (DirectShow)");
#elif defined(Q_OS_MAC)
    return QStringLiteral("Caméra USB (AVFoundation)");
#else
    return QStringLiteral("Caméra USB");
#endif
}

QVector<CameraDeviceInfo> UvcCameraBackend::enumerateDevices()
{
    if (isOpen())
        return {};
    return probeDevices();
}

QVector<CameraDeviceInfo> UvcCameraBackend::probeDevices()
{
    // No reliable way to ask the OS for a device's friendly capability set
    // without opening it, so this probes each index directly: request a
    // large resolution and see what actually comes back. Returns every
    // camera that responds (e.g. the microscope AND a laptop's built-in
    // webcam if both are present) — CameraManager decides which one (if
    // any) to auto-connect to, applying the "must look like the microscope"
    // safety threshold itself; this layer just reports what's out there so
    // a teacher can manually pick a specific device if needed.
    QVector<CameraDeviceInfo> devices;

    for (int i = 0; i < kMaxProbedIndex; ++i) {
        cv::VideoCapture probe(i, kCaptureBackend);
        if (!probe.isOpened())
            continue;

        probe.set(cv::CAP_PROP_FRAME_WIDTH, 4000);
        probe.set(cv::CAP_PROP_FRAME_HEIGHT, 3000);

        // Measure the actual captured frame, not the pre-capture .get()
        // properties (which can report the requested size rather than what
        // the driver actually negotiated).
        cv::Mat frame;
        const bool gotFrame = probe.read(frame) && !frame.empty();
        probe.release();
        if (!gotFrame)
            continue;

        CameraDeviceInfo info;
        info.id = QString::number(i);
        info.resolution = QSize(frame.cols, frame.rows);
        info.displayName = QStringLiteral("Caméra USB (%1x%2)").arg(frame.cols).arg(frame.rows);
        devices.append(info);
    }

    return devices;
}

bool UvcCameraBackend::open(const QString &deviceId)
{
    if (isOpen())
        return false;

    bool ok = false;
    const int index = deviceId.toInt(&ok);
    if (!ok)
        return false;

    auto capture = QScopedPointer<cv::VideoCapture>(new cv::VideoCapture(index, kCaptureBackend));
    if (!capture->isOpened())
        return false;

    // Default to a lower resolution than the microscope's native 2592x1944:
    // the full-res stream only delivers ~2 fps over USB, too choppy for a
    // live student view. Same 4:3 aspect ratio, so no cropping/distortion.
    // Full resolution is still reachable via setResolution() (teacher panel).
    capture->set(cv::CAP_PROP_FRAME_WIDTH, 1280);
    capture->set(cv::CAP_PROP_FRAME_HEIGHT, 960);

    // The driver's own default gain is 0, which renders as a near-black
    // image until someone manually raises it in the teacher panel — 35
    // gives a properly usable picture out of the box. Still adjustable via
    // setGain() afterwards.
    capture->set(cv::CAP_PROP_GAIN, 35);

    m_capture.reset(capture.take());
    m_openIndex = index;
    m_frameCountSinceFpsUpdate = 0;
    m_fpsClock.start();
    m_lastGoodFrameClock.start();
    m_captureTimer.start(kCaptureIntervalMs);

    CameraDeviceInfo info;
    info.id = deviceId;
    info.displayName = QStringLiteral("Caméra USB");
    emit deviceConnected(info);
    return true;
}

void UvcCameraBackend::close()
{
    if (!isOpen())
        return;

    m_captureTimer.stop();
    m_capture->release();
    m_capture.reset();
    m_openIndex = -1;
    emit deviceDisconnected();
}

bool UvcCameraBackend::isOpen() const
{
    return m_capture && m_capture->isOpened();
}

void UvcCameraBackend::captureFrame()
{
    if (!isOpen())
        return;

    cv::Mat frame;
    if (!m_capture->read(frame) || frame.empty()) {
        // Tolerate transient read failures (e.g. the driver renegotiating
        // the stream right after setResolution()) instead of treating the
        // very first miss as a fatal disconnect.
        constexpr qint64 kStreamLostThresholdMs = 2500;
        if (m_lastGoodFrameClock.isValid() && m_lastGoodFrameClock.elapsed() < kStreamLostThresholdMs)
            return;
        emit errorOccurred(QStringLiteral("Perte du flux caméra."));
        close();
        return;
    }
    m_lastGoodFrameClock.restart();

    if (m_wbCalibrationRequested) {
        m_wbCalibrationRequested = false;
        const cv::Scalar avg = cv::mean(frame); // BGR order
        const double gray = (avg[0] + avg[1] + avg[2]) / 3.0;
        auto safeGain = [gray](double channelAvg) {
            return channelAvg < 1.0 ? 1.0 : qBound(0.3, gray / channelAvg, 3.0);
        };
        m_wbGainB = safeGain(avg[0]);
        m_wbGainG = safeGain(avg[1]);
        m_wbGainR = safeGain(avg[2]);
    }

    if (m_wbGainB != 1.0 || m_wbGainG != 1.0 || m_wbGainR != 1.0) {
        std::vector<cv::Mat> channels;
        cv::split(frame, channels);
        channels[0].convertTo(channels[0], -1, m_wbGainB, 0);
        channels[1].convertTo(channels[1], -1, m_wbGainG, 0);
        channels[2].convertTo(channels[2], -1, m_wbGainR, 0);
        cv::merge(channels, frame);
    }

    cv::Mat rgb;
    cv::cvtColor(frame, rgb, cv::COLOR_BGR2RGB);

    CameraFrame result;
    result.image = QImage(rgb.data, rgb.cols, rgb.rows, static_cast<int>(rgb.step), QImage::Format_RGB888).copy();
    result.timestampMs = static_cast<quint64>(m_fpsClock.elapsed());
    emit frameReady(result);

    ++m_frameCountSinceFpsUpdate;
    if (m_fpsClock.elapsed() >= 1000) {
        emit fpsUpdated(m_frameCountSinceFpsUpdate * 1000.0 / m_fpsClock.elapsed());
        m_frameCountSinceFpsUpdate = 0;
        m_fpsClock.restart();
    }
}

QSize UvcCameraBackend::currentResolution() const
{
    if (!isOpen())
        return {};
    return QSize(static_cast<int>(m_capture->get(cv::CAP_PROP_FRAME_WIDTH)),
                 static_cast<int>(m_capture->get(cv::CAP_PROP_FRAME_HEIGHT)));
}

QVector<QSize> UvcCameraBackend::supportedResolutions() const
{
    if (!isOpen())
        return {};
    // OpenCV's VideoCapture has no portable "list supported resolutions"
    // query; offering the negotiated one plus the microscope's native 4:3
    // presets is good enough for the teacher panel's dropdown.
    return {currentResolution(), QSize(2592, 1944), QSize(1280, 960), QSize(640, 480)};
}

bool UvcCameraBackend::setResolution(const QSize &size)
{
    if (!isOpen())
        return false;
    m_capture->set(cv::CAP_PROP_FRAME_WIDTH, size.width());
    m_capture->set(cv::CAP_PROP_FRAME_HEIGHT, size.height());
    return true;
}

bool UvcCameraBackend::setAutoWhiteBalance(bool enabled)
{
    if (!isOpen())
        return false;

    m_capture->set(cv::CAP_PROP_AUTO_WB, enabled ? 1 : 0); // best-effort, driver-dependent

    if (enabled) {
        // Calibrate from the next captured frame: point the camera at a
        // blank/bright area when clicking Auto for a correct reference.
        m_wbCalibrationRequested = true;
    } else {
        m_wbGainB = m_wbGainG = m_wbGainR = 1.0;
    }
    return true;
}

bool UvcCameraBackend::autoWhiteBalance() const
{
    if (!isOpen())
        return false;
    return m_capture->get(cv::CAP_PROP_AUTO_WB) != 0;
}

bool UvcCameraBackend::setAutoExposure(bool enabled)
{
    if (!isOpen())
        return false;
    // The auto-exposure convention here is famously inverted and
    // driver-dependent (commonly 0.75 = auto, 0.25 = manual); adjust here
    // if a given camera's behaviour is reversed.
    return m_capture->set(cv::CAP_PROP_AUTO_EXPOSURE, enabled ? 0.75 : 0.25);
}

bool UvcCameraBackend::autoExposure() const
{
    if (!isOpen())
        return false;
    return m_capture->get(cv::CAP_PROP_AUTO_EXPOSURE) >= 0.5;
}

bool UvcCameraBackend::setBrightness(int value0to100)
{
    if (!isOpen())
        return false;
    return m_capture->set(cv::CAP_PROP_BRIGHTNESS, value0to100);
}

int UvcCameraBackend::brightness() const
{
    if (!isOpen())
        return 0;
    return qBound(0, static_cast<int>(m_capture->get(cv::CAP_PROP_BRIGHTNESS)), 100);
}

bool UvcCameraBackend::setExposure(int value0to100)
{
    if (!isOpen())
        return false;
    return m_capture->set(cv::CAP_PROP_EXPOSURE, value0to100);
}

int UvcCameraBackend::exposure() const
{
    if (!isOpen())
        return 0;
    return qBound(0, static_cast<int>(m_capture->get(cv::CAP_PROP_EXPOSURE)), 100);
}

bool UvcCameraBackend::setGain(int value0to100)
{
    if (!isOpen())
        return false;
    return m_capture->set(cv::CAP_PROP_GAIN, value0to100);
}

int UvcCameraBackend::gain() const
{
    if (!isOpen())
        return 0;
    return qBound(0, static_cast<int>(m_capture->get(cv::CAP_PROP_GAIN)), 100);
}
