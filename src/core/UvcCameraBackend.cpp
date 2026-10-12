#include "UvcCameraBackend.h"

#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

#include <vector>

#include <QImage>
#include <QMutex>
#include <QMutexLocker>
#include <QThread>

#include "DirectShowInfo.h"

namespace {
constexpr int kMaxProbedIndex = 8;
// Never poll the camera faster than this (a backend that returns instantly
// with a repeated frame would otherwise spin a CPU core).
constexpr int kMinFrameIntervalMs = 8;

// What DirectShow said about each camera at the last scan. Filled on the
// probing worker thread, read on the UI thread when the resolution menu
// opens — enumerating again right then would stall the click for as long as
// the slowest driver takes to answer.
QMutex g_dsCacheMutex;
QVector<DirectShowDevice> g_dsCache;

QString nameForIndex(int index)
{
    QMutexLocker lock(&g_dsCacheMutex);
    for (const DirectShowDevice &device : g_dsCache) {
        if (device.index == index)
            return device.name;
    }
    return {};
}

QVector<QSize> modesForIndex(int index)
{
    {
        QMutexLocker lock(&g_dsCacheMutex);
        for (const DirectShowDevice &device : g_dsCache) {
            if (device.index == index)
                return device.resolutions;
        }
    }
    const QVector<DirectShowDevice> fresh = enumerateDirectShowDevices();
    QMutexLocker lock(&g_dsCacheMutex);
    g_dsCache = fresh;
    for (const DirectShowDevice &device : g_dsCache) {
        if (device.index == index)
            return device.resolutions;
    }
    return {};
}

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
    // Frames cross from the capture thread to the GUI thread by queued call.
    qRegisterMetaType<CameraFrame>("CameraFrame");
}

UvcCameraBackend::~UvcCameraBackend()
{
    close();
}

QString UvcCameraBackend::backendName() const
{
#if defined(Q_OS_WIN)
    return tr("Caméra USB (DirectShow)");
#elif defined(Q_OS_MAC)
    return tr("Caméra USB (AVFoundation)");
#else
    return tr("Caméra USB");
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
        info.displayName = tr("Caméra USB (%1x%2)").arg(frame.cols).arg(frame.rows);
        devices.append(info);
    }

    // The driver's own name for each camera (and the modes it really
    // supports), matched to OpenCV's index — the reliable way to tell the
    // microscope from the laptop's webcam, instead of guessing from image
    // size alone.
    const QVector<DirectShowDevice> dsDevices = enumerateDirectShowDevices();
    {
        QMutexLocker lock(&g_dsCacheMutex);
        g_dsCache = dsDevices;
    }
    for (CameraDeviceInfo &info : devices) {
        const int index = info.id.toInt();
        for (const DirectShowDevice &ds : dsDevices) {
            if (ds.index != index)
                continue;
            info.deviceName = ds.name;
            if (!ds.name.isEmpty())
                info.displayName = tr("%1 (%2x%3)").arg(ds.name).arg(info.resolution.width()).arg(info.resolution.height());
            break;
        }
    }

    return devices;
}

bool UvcCameraBackend::openCaptureAt(int index, const QSize &size)
{
    auto capture = QScopedPointer<cv::VideoCapture>(new cv::VideoCapture(index, kCaptureBackend));
    if (!capture->isOpened())
        return false;

    capture->set(cv::CAP_PROP_FRAME_WIDTH, size.width());
    capture->set(cv::CAP_PROP_FRAME_HEIGHT, size.height());

    // Deliberately not touching auto-exposure/auto-WB here: the camera
    // starts in whatever mode it had, and the teacher panel's checkboxes
    // (setAutoExposure(), setAutoWhiteBalance()) switch it explicitly.
    m_capture.reset(capture.take());
    m_openIndex = index;
    return true;
}

bool UvcCameraBackend::open(const QString &deviceId)
{
    if (isOpen())
        return false;

    bool ok = false;
    const int index = deviceId.toInt(&ok);
    if (!ok)
        return false;

    // Default to a lower resolution than the microscope's native 2592x1944:
    // the full-res stream only delivers ~2 fps over USB, too choppy for a
    // live student view. Same 4:3 aspect ratio, so no cropping/distortion.
    // Full resolution is still reachable via setResolution() (teacher panel).
    if (!openCaptureAt(index, QSize(1280, 960)))
        return false;

    applyDefaultGain();
    startWorker();

    CameraDeviceInfo info;
    info.id = deviceId;
    info.displayName = tr("Caméra USB");
    info.deviceName = nameForIndex(index);
    info.model = info.deviceName;
    emit deviceConnected(info);
    return true;
}

void UvcCameraBackend::close()
{
    if (!isOpen())
        return;

    stopWorker();
    {
        QMutexLocker lock(&m_captureMutex);
        m_capture->release();
        m_capture.reset();
    }
    m_realControls.reset();
    m_openIndex = -1;
    emit deviceDisconnected();
}

bool UvcCameraBackend::isOpen() const
{
    return m_capture && m_capture->isOpened();
}

void UvcCameraBackend::startWorker()
{
    stopWorker();
    m_stopRequested = false;
    const int generation = ++m_generation;
    {
        QMutexLocker lock(&m_frameMutex);
        m_deliveryPending = false;
        m_latestFrame = CameraFrame();
    }
    m_thread = QThread::create([this, generation]() { captureLoop(generation); });
    m_thread->start();
}

void UvcCameraBackend::stopWorker()
{
    if (!m_thread)
        return;
    m_stopRequested = true;
    m_thread->wait();
    delete m_thread;
    m_thread = nullptr;
}

void UvcCameraBackend::captureLoop(int generation)
{
    // Wall-clock trouble tolerance: a UVC driver commonly drops a handful of
    // reads while it renegotiates the stream - normal settling, not a real
    // disconnect. Only declare the stream lost after it stays silent for a
    // sustained stretch (by time, so it scales with whatever fps there is).
    constexpr qint64 kStreamLostThresholdMs = 2500;

    QElapsedTimer sessionClock;
    sessionClock.start();
    QElapsedTimer fpsClock;
    fpsClock.start();
    QElapsedTimer lastGoodFrame;
    lastGoodFrame.start();
    QElapsedTimer pace;
    int framesSinceFpsUpdate = 0;

    while (!m_stopRequested) {
        pace.restart();

        cv::Mat frame;
        bool gotFrame = false;
        {
            QMutexLocker lock(&m_captureMutex);
            if (!m_capture || !m_capture->isOpened())
                return;
            gotFrame = m_capture->read(frame) && !frame.empty();
        }

        if (!gotFrame) {
            if (lastGoodFrame.elapsed() >= kStreamLostThresholdMs) {
                QMetaObject::invokeMethod(this, [this, generation]() { onStreamLost(generation); },
                                          Qt::QueuedConnection);
                return;
            }
            QThread::msleep(5);
            continue;
        }
        lastGoodFrame.restart();

        double gainB = 1.0, gainG = 1.0, gainR = 1.0;
        {
            QMutexLocker lock(&m_wbMutex);
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
            gainB = m_wbGainB;
            gainG = m_wbGainG;
            gainR = m_wbGainR;
        }

        if (gainB != 1.0 || gainG != 1.0 || gainR != 1.0) {
            std::vector<cv::Mat> channels;
            cv::split(frame, channels);
            channels[0].convertTo(channels[0], -1, gainB, 0);
            channels[1].convertTo(channels[1], -1, gainG, 0);
            channels[2].convertTo(channels[2], -1, gainR, 0);
            cv::merge(channels, frame);
        }

        cv::Mat rgb;
        cv::cvtColor(frame, rgb, cv::COLOR_BGR2RGB);

        CameraFrame result;
        result.image = QImage(rgb.data, rgb.cols, rgb.rows, static_cast<int>(rgb.step), QImage::Format_RGB888).copy();
        result.timestampMs = static_cast<quint64>(sessionClock.elapsed());

        bool needsDelivery = false;
        {
            QMutexLocker lock(&m_frameMutex);
            m_latestFrame = result;
            needsDelivery = !m_deliveryPending;
            m_deliveryPending = true;
        }
        if (needsDelivery)
            QMetaObject::invokeMethod(this, [this]() { deliverLatestFrame(); }, Qt::QueuedConnection);

        ++framesSinceFpsUpdate;
        if (fpsClock.elapsed() >= 1000) {
            emit fpsUpdated(framesSinceFpsUpdate * 1000.0 / fpsClock.elapsed());
            framesSinceFpsUpdate = 0;
            fpsClock.restart();
        }

        const qint64 spent = pace.elapsed();
        if (spent < kMinFrameIntervalMs)
            QThread::msleep(static_cast<unsigned long>(kMinFrameIntervalMs - spent));
    }
}

void UvcCameraBackend::deliverLatestFrame()
{
    CameraFrame frame;
    {
        QMutexLocker lock(&m_frameMutex);
        frame = m_latestFrame;
        m_deliveryPending = false;
    }
    if (isOpen() && !frame.image.isNull())
        emit frameReady(frame);
}

void UvcCameraBackend::onStreamLost(int generation)
{
    if (generation != m_generation || !isOpen())
        return;
    emit errorOccurred(tr("Perte du flux caméra."));
    close();
}

QSize UvcCameraBackend::currentResolution() const
{
    if (!isOpen())
        return {};
    QMutexLocker lock(&m_captureMutex);
    return QSize(static_cast<int>(m_capture->get(cv::CAP_PROP_FRAME_WIDTH)),
                 static_cast<int>(m_capture->get(cv::CAP_PROP_FRAME_HEIGHT)));
}

QVector<QSize> UvcCameraBackend::supportedResolutions() const
{
    if (!isOpen())
        return {};
    // OpenCV's VideoCapture can't list a camera's modes, so ask DirectShow
    // for the ones this driver really supports (largest first, small
    // thumbnail-sized modes dropped, capped so the menu stays short).
    QVector<QSize> modes;
    for (const QSize &size : modesForIndex(m_openIndex)) {
        if (size.width() >= 640)
            modes.append(size);
    }
    if (modes.size() > 6)
        modes.resize(6);
    if (!modes.isEmpty())
        return modes;

    // DirectShow gave nothing usable (non-Windows, or a driver that hides
    // its modes): fall back to the microscope's three usual 4:3 presets.
    return {QSize(2592, 1944), QSize(1280, 960), QSize(640, 480)};
}

bool UvcCameraBackend::setResolution(const QSize &size)
{
    if (!isOpen())
        return false;

    // A plain property set on the already-open capture (tried here
    // before, even with frame reads paused around it) does not get this
    // driver to actually renegotiate the frame size — confirmed by testing,
    // it just silently keeps streaming at whatever resolution it already
    // had. Reopening the device is the only thing that actually works, but
    // an earlier attempt at that had a real bug: it constructed the new
    // cv::VideoCapture BEFORE releasing the old one. With the old handle
    // still holding the device, opening "the same index" again raced
    // DirectShow's enumeration and could silently hand back a DIFFERENT
    // physical camera (the computer's own webcam) instead of the
    // microscope. Explicitly releasing first — so the device is actually
    // free before reopening it at the new size — is the fix; the physical
    // device index itself (m_openIndex) is unaffected by that release.
    stopWorker();
    const int index = m_openIndex;
    {
        QMutexLocker lock(&m_captureMutex);
        m_capture->release();
        m_capture.reset();
    }

    if (!openCaptureAt(index, size)) {
        m_realControls.reset();
        m_openIndex = -1;
        emit deviceDisconnected();
        return false;
    }

    startWorker();
    return true;
}

bool UvcCameraBackend::setAutoWhiteBalance(bool enabled)
{
    if (!isOpen())
        return false;

    {
        QMutexLocker lock(&m_captureMutex);
        m_capture->set(cv::CAP_PROP_AUTO_WB, enabled ? 1 : 0); // best-effort, driver-dependent
    }

    QMutexLocker wbLock(&m_wbMutex);
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
    QMutexLocker lock(&m_captureMutex);
    return m_capture->get(cv::CAP_PROP_AUTO_WB) != 0;
}

DirectShowControls *UvcCameraBackend::realControls() const
{
    if (!isOpen())
        return nullptr;
    if (!m_realControls)
        m_realControls.reset(new DirectShowControls(m_openIndex));
    return m_realControls->isValid() ? m_realControls.get() : nullptr;
}

namespace {
// The UI speaks 0-100; the driver speaks its own units (exposure is usually
// log2 seconds like -10..-4, gain 1..8, ...). Linear in the driver's units.
int rawToPercent(const DsControlRange &range, long raw)
{
    const double fraction = static_cast<double>(raw - range.min) / static_cast<double>(range.max - range.min);
    return qBound(0, qRound(fraction * 100.0), 100);
}

long percentToRaw(const DsControlRange &range, int percent)
{
    const double raw = range.min + (range.max - range.min) * qBound(0, percent, 100) / 100.0;
    const long snapped = range.min + qRound((raw - range.min) / range.step) * range.step;
    return qBound(range.min, snapped, range.max);
}

// Real range of a control if the driver exposes it, else an empty one.
DsControlRange usableRange(DirectShowControls *controls, DsControl control)
{
    return controls ? controls->range(control) : DsControlRange();
}
}

void UvcCameraBackend::applyDefaultGain()
{
    // The driver's own default gain is often the minimum, which renders as a
    // near-black image until someone raises it in the teacher panel. 35
    // gives a properly usable picture out of the box on the OMAX — as a raw
    // value when the camera's real range contains it, otherwise 35% of the
    // range (a webcam's gain only goes 1..8, where raw 35 would just mean
    // "maximum"). Done once per connection, not on every reconfiguration, so
    // a gain the teacher chose is never silently reset.
    const DsControlRange range = usableRange(realControls(), DsControl::Gain);
    if (range.available) {
        long current = range.defaultValue;
        bool isAuto = false;
        realControls()->read(DsControl::Gain, current, isAuto);
        if (current <= range.min) {
            const long target = (35 >= range.min && 35 <= range.max)
                ? 35 : range.min + qRound((range.max - range.min) * 0.35);
            realControls()->writeManual(DsControl::Gain, target);
        }
        return;
    }
    QMutexLocker lock(&m_captureMutex);
    m_capture->set(cv::CAP_PROP_GAIN, 35);
}

bool UvcCameraBackend::setAutoExposure(bool enabled)
{
    if (!isOpen())
        return false;

    DirectShowControls *controls = realControls();
    const DsControlRange range = usableRange(controls, DsControl::Exposure);
    if (range.available && range.autoSupported) {
        if (enabled)
            return controls->writeAuto(DsControl::Exposure);
        // Leaving auto: freeze at whatever exposure the camera had settled on
        // instead of jumping to some arbitrary value.
        long value = range.defaultValue;
        bool isAuto = false;
        controls->read(DsControl::Exposure, value, isAuto);
        return controls->writeManual(DsControl::Exposure, value);
    }
    // Fallback (OpenCV's DirectShow convention, confirmed on real hardware:
    // 0.75 = auto, 0.25 = manual).
    QMutexLocker lock(&m_captureMutex);
    return m_capture->set(cv::CAP_PROP_AUTO_EXPOSURE, enabled ? 0.75 : 0.25);
}

bool UvcCameraBackend::autoExposure() const
{
    if (!isOpen())
        return false;

    DirectShowControls *controls = realControls();
    if (usableRange(controls, DsControl::Exposure).available) {
        long value = 0;
        bool isAuto = false;
        if (controls->read(DsControl::Exposure, value, isAuto))
            return isAuto;
    }
    QMutexLocker lock(&m_captureMutex);
    return m_capture->get(cv::CAP_PROP_AUTO_EXPOSURE) >= 0.5;
}

bool UvcCameraBackend::setExposure(int value0to100)
{
    if (!isOpen())
        return false;

    DirectShowControls *controls = realControls();
    const DsControlRange range = usableRange(controls, DsControl::Exposure);
    if (range.available)
        return controls->writeManual(DsControl::Exposure, percentToRaw(range, value0to100));
    QMutexLocker lock(&m_captureMutex);
    return m_capture->set(cv::CAP_PROP_EXPOSURE, value0to100);
}

int UvcCameraBackend::exposure() const
{
    if (!isOpen())
        return 0;

    DirectShowControls *controls = realControls();
    const DsControlRange range = usableRange(controls, DsControl::Exposure);
    if (range.available) {
        long value = 0;
        bool isAuto = false;
        if (controls->read(DsControl::Exposure, value, isAuto))
            return rawToPercent(range, value);
    }
    QMutexLocker lock(&m_captureMutex);
    return qBound(0, static_cast<int>(m_capture->get(cv::CAP_PROP_EXPOSURE)), 100);
}

bool UvcCameraBackend::setGain(int value0to100)
{
    if (!isOpen())
        return false;

    DirectShowControls *controls = realControls();
    const DsControlRange range = usableRange(controls, DsControl::Gain);
    if (range.available)
        return controls->writeManual(DsControl::Gain, percentToRaw(range, value0to100));
    QMutexLocker lock(&m_captureMutex);
    return m_capture->set(cv::CAP_PROP_GAIN, value0to100);
}

int UvcCameraBackend::gain() const
{
    if (!isOpen())
        return 0;

    DirectShowControls *controls = realControls();
    const DsControlRange range = usableRange(controls, DsControl::Gain);
    if (range.available) {
        long value = 0;
        bool isAuto = false;
        if (controls->read(DsControl::Gain, value, isAuto))
            return rawToPercent(range, value);
    }
    QMutexLocker lock(&m_captureMutex);
    return qBound(0, static_cast<int>(m_capture->get(cv::CAP_PROP_GAIN)), 100);
}
