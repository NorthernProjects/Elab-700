#pragma once

#include <QImage>
#include <QMetaType>
#include <QSize>
#include <QString>
#include <QVector>

// Plain data types shared between CameraBackend implementations and the UI.
// Kept free of any vendor SDK types so the UI never depends on ToupCam.

struct CameraDeviceInfo {
    QString id;          // backend-specific unique id (e.g. ToupCam device id)
    QString displayName; // human readable name shown in the teacher panel
    QString model;       // e.g. "SCMOS05000KPA"
    QSize resolution;     // negotiated resolution at enumeration time, for the manual device picker
};

struct CameraFrame {
    QImage image;
    quint64 timestampMs = 0;
};

// Exposure/white balance/brightness use a normalized 0-100 range in the UI;
// each backend maps that range to whatever units the real hardware expects.
struct CameraRange {
    int min = 0;
    int max = 100;
    int defaultValue = 50;
};

Q_DECLARE_METATYPE(CameraDeviceInfo)
Q_DECLARE_METATYPE(CameraFrame)
