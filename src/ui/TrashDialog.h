#pragma once

#include <QDialog>

class QListWidget;

// Shows whatever is currently sitting in a group's "Corbeille" subfolder
// (see GalleryModel::trashFile) with a chance to restore a photo/video back
// into the gallery, or delete it for good. Purely a recovery net for
// accidental "Supprimer" clicks — not a full undo history.
class TrashDialog : public QDialog {
    Q_OBJECT

public:
    TrashDialog(const QString &captureFolder, QWidget *parent = nullptr);

private slots:
    void onRestoreSelected();
    void onDeleteForeverSelected();
    void reloadItems();

private:
    QString m_captureFolder;
    QListWidget *m_listWidget;
    class QPushButton *m_restoreButton;
    class QPushButton *m_deleteButton;
};
