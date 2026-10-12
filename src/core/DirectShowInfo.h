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
