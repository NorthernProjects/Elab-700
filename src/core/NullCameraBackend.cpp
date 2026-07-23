#include "NullCameraBackend.h"

NullCameraBackend::NullCameraBackend(QObject *parent) : CameraBackend(parent) {}

QString NullCameraBackend::backendName() const { return QStringLiteral("Aucune caméra (SDK non installé)"); }

QVector<CameraDeviceInfo> NullCameraBackend::enumerateDevices() { return {}; }

bool NullCameraBackend::open(const QString & /*deviceId*/) { return false; }

void NullCameraBackend::close() {}

bool NullCameraBackend::isOpen() const { return false; }

QSize NullCameraBackend::currentResolution() const { return {}; }

QVector<QSize> NullCameraBackend::supportedResolutions() const { return {}; }

bool NullCameraBackend::setResolution(const QSize & /*size*/) { return false; }

bool NullCameraBackend::setAutoWhiteBalance(bool /*enabled*/) { return false; }

bool NullCameraBackend::autoWhiteBalance() const { return false; }

bool NullCameraBackend::setAutoExposure(bool /*enabled*/) { return false; }

bool NullCameraBackend::autoExposure() const { return false; }

bool NullCameraBackend::setBrightness(int /*value0to100*/) { return false; }

int NullCameraBackend::brightness() const { return 0; }

bool NullCameraBackend::setExposure(int /*value0to100*/) { return false; }

int NullCameraBackend::exposure() const { return 0; }

bool NullCameraBackend::setGain(int /*value0to100*/) { return false; }

int NullCameraBackend::gain() const { return 0; }
