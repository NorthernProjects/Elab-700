#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QString>
#include <QVector>

// Lists photo/video files found in the current capture folder, newest first.
// Read-only model: capturing is CaptureManager's job, this class only scans
// and reports what already exists on disk (for the gallery dialog).
class GalleryModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles {
        FilePathRole = Qt::UserRole + 1,
        IsVideoRole,
        CapturedAtRole,
    };

    struct Entry {
        QString filePath;
        bool isVideo = false;
        QDateTime capturedAt;
    };

    explicit GalleryModel(QObject *parent = nullptr);

    void setFolder(const QString &folder);
    QString folder() const { return m_folder; }

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Entry entryAt(int row) const;
    bool removeFile(int row);

    // Moves the file into a "Corbeille" subfolder of the current capture
    // folder instead of deleting it outright — recoverable for a while (see
    // purgeOldTrash) rather than gone the instant someone taps "Supprimer".
    bool trashFile(int row);

    static QString trashFolderName();

    // Permanently removes anything sitting in a folder's Corbeille subfolder
    // for longer than maxAgeDays — called opportunistically (e.g. whenever
    // the gallery is opened) rather than on a background timer.
    static void purgeOldTrash(const QString &captureFolder, int maxAgeDays);

public slots:
    void refresh();

private:
    QString m_folder;
    QVector<Entry> m_entries;
};
