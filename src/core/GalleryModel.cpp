#include "GalleryModel.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace {
const QStringList kPhotoFilters = {"*.png", "*.jpg", "*.jpeg", "*.tif", "*.tiff"};
const QStringList kVideoFilters = {"*.mp4", "*.avi"};
const QString kTrashFolderName = QStringLiteral("Corbeille");
}

GalleryModel::GalleryModel(QObject *parent) : QAbstractListModel(parent) {}

void GalleryModel::setFolder(const QString &folder)
{
    m_folder = folder;
    refresh();
}

void GalleryModel::refresh()
{
    beginResetModel();
    m_entries.clear();

    QDir dir(m_folder);
    if (dir.exists()) {
        const QStringList allFilters = kPhotoFilters + kVideoFilters;
        const QFileInfoList files = dir.entryInfoList(allFilters, QDir::Files, QDir::Time);
        for (const QFileInfo &fi : files) {
            Entry entry;
            entry.filePath = fi.absoluteFilePath();
            entry.isVideo = kVideoFilters.contains("*." + fi.suffix().toLower());
            entry.capturedAt = fi.birthTime().isValid() ? fi.birthTime() : fi.lastModified();
            m_entries.append(entry);
        }
    }

    endResetModel();
}

int GalleryModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return m_entries.size();
}

QVariant GalleryModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size())
        return {};

    const Entry &entry = m_entries.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
        return QFileInfo(entry.filePath).fileName();
    case FilePathRole:
        return entry.filePath;
    case IsVideoRole:
        return entry.isVideo;
    case CapturedAtRole:
        return entry.capturedAt;
    default:
        return {};
    }
}

QHash<int, QByteArray> GalleryModel::roleNames() const
{
    return {
        {Qt::DisplayRole, "display"},
        {FilePathRole, "filePath"},
        {IsVideoRole, "isVideo"},
        {CapturedAtRole, "capturedAt"},
    };
}

GalleryModel::Entry GalleryModel::entryAt(int row) const
{
    if (row < 0 || row >= m_entries.size())
        return {};
    return m_entries.at(row);
}

bool GalleryModel::removeFile(int row)
{
    if (row < 0 || row >= m_entries.size())
        return false;

    const QString path = m_entries.at(row).filePath;
    if (!QFile::remove(path))
        return false;

    beginRemoveRows(QModelIndex(), row, row);
    m_entries.remove(row);
    endRemoveRows();
    return true;
}

QString GalleryModel::trashFolderName()
{
    return kTrashFolderName;
}

bool GalleryModel::trashFile(int row)
{
    if (row < 0 || row >= m_entries.size())
        return false;

    const QString path = m_entries.at(row).filePath;
    QDir dir(m_folder);
    if (!dir.mkpath(kTrashFolderName))
        return false;

    const QFileInfo fi(path);
    QDir trashDir(dir.filePath(kTrashFolderName));
    QString destPath = trashDir.filePath(fi.fileName());
    int counter = 1;
    while (QFile::exists(destPath)) {
        destPath = trashDir.filePath(QStringLiteral("%1_%2.%3")
                                          .arg(fi.completeBaseName())
                                          .arg(counter++)
                                          .arg(fi.suffix()));
    }

    if (!QFile::rename(path, destPath))
        return false;

    // The move itself doesn't update the file's modified time on most
    // filesystems (it still reflects when the photo/video was originally
    // captured) — bump it to now so purgeOldTrash actually measures time
    // spent in the trash, not time since capture.
    QFile trashedFile(destPath);
    if (trashedFile.open(QIODevice::ReadWrite)) {
        trashedFile.setFileTime(QDateTime::currentDateTime(), QFileDevice::FileModificationTime);
        trashedFile.close();
    }

    beginRemoveRows(QModelIndex(), row, row);
    m_entries.remove(row);
    endRemoveRows();
    return true;
}

void GalleryModel::purgeOldTrash(const QString &captureFolder, int maxAgeDays)
{
    const QDir trashDir(QDir(captureFolder).filePath(kTrashFolderName));
    if (!trashDir.exists())
        return;

    const QDateTime cutoff = QDateTime::currentDateTime().addDays(-maxAgeDays);
    const QFileInfoList files = trashDir.entryInfoList(QDir::Files);
    for (const QFileInfo &fi : files) {
        if (fi.lastModified() < cutoff)
            QFile::remove(fi.absoluteFilePath());
    }
}
