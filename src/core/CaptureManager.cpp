#include "CaptureManager.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QTextStream>

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/videoio.hpp>

#include "AppSettings.h"

namespace {

bool imageToBgrMat(const QImage &image, cv::Mat &out)
{
    if (image.isNull())
        return false;

    const QImage converted = image.convertToFormat(QImage::Format_RGB888);
    const cv::Mat rgb(converted.height(), converted.width(), CV_8UC3,
                       const_cast<uchar *>(converted.bits()), static_cast<size_t>(converted.bytesPerLine()));
    cv::cvtColor(rgb, out, cv::COLOR_RGB2BGR);
    return true;
}

QString sanitizeForFileName(const QString &name)
{
    QString result = name.trimmed();
    for (const QChar &forbidden : QStringLiteral("\\/:*?\"<>|"))
        result.replace(forbidden, QLatin1Char('_'));
    return result;
}

}

// Lives on CaptureManager's dedicated worker thread: all cv::VideoWriter
// access (open/write/release) happens here, off the GUI thread, so encoding
// a frame can never stall a paint event on the main window.
class VideoWriterWorker : public QObject {
    Q_OBJECT

public slots:
    bool open(const QString &path, double fps, int width, int height)
    {
        const int fourcc = cv::VideoWriter::fourcc('M', 'J', 'P', 'G');
        return m_writer.open(path.toStdString(), fourcc, fps, cv::Size(width, height), true);
    }

    void writeFrame(const QImage &image)
    {
        if (!m_writer.isOpened())
            return;
        cv::Mat mat;
        if (imageToBgrMat(image, mat))
            m_writer.write(mat);
    }

    void close()
    {
        if (m_writer.isOpened())
            m_writer.release();
    }

private:
    cv::VideoWriter m_writer;
};

CaptureManager::CaptureManager(AppSettings *settings, QObject *parent)
    : QObject(parent), m_settings(settings)
{
    m_writerWorker = new VideoWriterWorker();
    m_writerWorker->moveToThread(&m_writerThread);
    connect(&m_writerThread, &QThread::finished, m_writerWorker, &QObject::deleteLater);
    m_writerThread.start();
}

CaptureManager::~CaptureManager()
{
    if (isRecording())
        stopRecording();
    m_writerThread.quit();
    m_writerThread.wait();
}

void CaptureManager::pushFrame(const CameraFrame &frame)
{
    m_lastFrame = frame;

    if (m_recording) {
        QMetaObject::invokeMethod(m_writerWorker, "writeFrame", Qt::QueuedConnection,
                                   Q_ARG(QImage, frame.image));
    }
}

QString CaptureManager::nextFilePath(const QString &prefix, const QString &extension) const
{
    const QString folder = m_settings->activeCaptureFolder();
    QDir().mkpath(folder);
    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
    const QString group = sanitizeForFileName(m_settings->currentGroupName());

    const QString baseName = group.isEmpty() ? QStringLiteral("%1_%2").arg(prefix, timestamp)
                                              : QStringLiteral("%1_%2_%3").arg(group, prefix, timestamp);
    return QDir(folder).filePath(baseName + QLatin1Char('.') + extension);
}

void CaptureManager::takePhoto()
{
    if (m_lastFrame.image.isNull()) {
        emit captureError(QStringLiteral("Aucune image disponible : caméra non connectée."));
        return;
    }

    cv::Mat mat;
    if (!imageToBgrMat(m_lastFrame.image, mat)) {
        emit captureError(QStringLiteral("Impossible de convertir l'image pour l'enregistrement."));
        return;
    }

    const QString path = nextFilePath(QStringLiteral("photo"), m_settings->photoFormat());
    if (!cv::imwrite(path.toStdString(), mat)) {
        emit captureError(QStringLiteral("Échec de l'enregistrement de la photo : %1").arg(path));
        return;
    }

    if (m_settings->saveCaptureMetadata()) {
        // Same base name, .txt extension: photo_x.png -> photo_x.txt. A
        // failed sidecar write is silently ignored — the photo itself (the
        // thing that matters) is already safely on disk at this point.
        const QString sidecarPath = path.left(path.lastIndexOf(QLatin1Char('.'))) + QStringLiteral(".txt");
        QFile sidecar(sidecarPath);
        if (sidecar.open(QFile::WriteOnly | QFile::Text)) {
            QTextStream out(&sidecar);
            out << QStringLiteral("Fichier : %1\n").arg(QFileInfo(path).fileName());
            out << QStringLiteral("Date : %1\n")
                       .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
            out << QStringLiteral("Dimensions : %1 x %2 px\n")
                       .arg(m_lastFrame.image.width()).arg(m_lastFrame.image.height());
            if (m_metadataProvider)
                out << m_metadataProvider();
        }
    }

    emit photoSaved(path);
}

bool CaptureManager::startRecording()
{
    if (m_recording)
        return true;

    if (m_lastFrame.image.isNull()) {
        emit captureError(QStringLiteral("Aucune image disponible : caméra non connectée."));
        return false;
    }

    m_recordingPath = nextFilePath(QStringLiteral("video"), QStringLiteral("avi"));

    bool opened = false;
    QMetaObject::invokeMethod(m_writerWorker, "open", Qt::BlockingQueuedConnection,
                               Q_RETURN_ARG(bool, opened), Q_ARG(QString, m_recordingPath),
                               Q_ARG(double, 25.0), Q_ARG(int, m_lastFrame.image.width()),
                               Q_ARG(int, m_lastFrame.image.height()));
    if (!opened) {
        emit captureError(QStringLiteral("Impossible de démarrer l'enregistrement vidéo."));
        return false;
    }

    m_recording = true;
    return true;
}

void CaptureManager::stopRecording()
{
    if (!m_recording)
        return;

    m_recording = false;
    QMetaObject::invokeMethod(m_writerWorker, "close", Qt::BlockingQueuedConnection);
    emit recordingStopped(m_recordingPath);
}

#include "CaptureManager.moc"
