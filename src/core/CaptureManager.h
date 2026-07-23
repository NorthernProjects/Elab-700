#pragma once

#include <functional>

#include <QObject>
#include <QScopedPointer>
#include <QString>
#include <QThread>

#include "CameraTypes.h"

class AppSettings;
class VideoWriterWorker;

// Owns photo/video capture to disk. Receives frames pushed by MainWindow
// (already displayed by VideoView) and writes them out on request; never
// pulls frames from the camera directly so it stays backend-agnostic.
// Video encoding (cv::VideoWriter::write, which does MJPEG compression plus
// disk I/O) runs on a dedicated worker thread instead of the GUI thread —
// on a slow classroom PC or disk, encoding every frame inline here would
// stall paint events and make the live view stutter while recording.
class CaptureManager : public QObject {
    Q_OBJECT

public:
    explicit CaptureManager(AppSettings *settings, QObject *parent = nullptr);
    ~CaptureManager() override;

    bool isRecording() const { return m_recording; }

    // Extra lines appended to the .txt metadata sidecar written next to each
    // photo when AppSettings::saveCaptureMetadata() is on. Set by MainWindow,
    // which can read live camera state (exposure, gain, resolution) this
    // class deliberately has no access to (it stays backend-agnostic).
    void setMetadataProvider(std::function<QString()> provider) { m_metadataProvider = std::move(provider); }

public slots:
    void pushFrame(const CameraFrame &frame);
    void takePhoto();
    bool startRecording();
    void stopRecording();

signals:
    void photoSaved(const QString &path);
    void recordingStopped(const QString &path);
    void captureError(const QString &message);

private:
    QString nextFilePath(const QString &prefix, const QString &extension) const;

    AppSettings *m_settings;
    std::function<QString()> m_metadataProvider;
    CameraFrame m_lastFrame;
    bool m_recording = false;
    QString m_recordingPath;
    QThread m_writerThread;
    VideoWriterWorker *m_writerWorker;
};
