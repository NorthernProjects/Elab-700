#pragma once

#include <QSize>
#include <QString>
#include <QVector>

// One video-capture device as DirectShow itself reports it — the same
// enumeration (and the same order) OpenCV's CAP_DSHOW backend uses for its
// integer camera index, so `index` here is directly usable as that index.
struct DirectShowDevice {
    int index = -1;
    QString name;       // friendly name, e.g. "USB2.0 PC CAMERA"
    QString devicePath; // contains the USB vid/pid, handy to tell cameras apart
    // Every frame size the driver says it can really produce, largest first.
    QVector<QSize> resolutions;
};

// Windows only (empty elsewhere). Read-only: binds each device to inspect it
// but never starts a stream, so it does not light up or disturb the camera.
// Safe to call from any thread (initializes COM for the calling thread).
QVector<DirectShowDevice> enumerateDirectShowDevices();

// A cheap fingerprint of the camera list (names + USB paths only — the
// devices are not even bound, so nothing is opened). Changes when a camera is
// plugged in or removed. "none" when no camera exists; empty string when the
// platform can't tell (callers must then assume "changed").
QString directShowDeviceSignature();

enum class DsControl { Exposure, Gain, Brightness, WhiteBalance };

struct DsControlRange {
    bool available = false;
    long min = 0;
    long max = 0;
    long step = 1;
    long defaultValue = 0;
    bool autoSupported = false;
};

// The camera's real sliders (exposure, gain, brightness, white balance) with
// the real units and limits the driver reports — OpenCV's generic 0-100
// guesses are wrong for most cameras (exposure is typically log2 seconds,
// e.g. -13..-1). Bound to one camera by its OpenCV index; use from the GUI
// thread only (holds a COM object). Windows only; elsewhere isValid() is
// always false and every call fails harmlessly.
class DirectShowControls {
public:
    explicit DirectShowControls(int cameraIndex);
    ~DirectShowControls();
    DirectShowControls(const DirectShowControls &) = delete;
    DirectShowControls &operator=(const DirectShowControls &) = delete;

    bool isValid() const;
    DsControlRange range(DsControl control) const;

    // Current raw value and whether the driver is driving it automatically.
    bool read(DsControl control, long &value, bool &isAuto) const;
    // Switches that control to manual and sets the raw value.
    bool writeManual(DsControl control, long value);
    // Hands that control back to the camera's own automatic mode.
    bool writeAuto(DsControl control);

private:
    struct Impl;
    Impl *d;
};
