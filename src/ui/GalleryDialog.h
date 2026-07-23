#pragma once

#include <QDialog>

#include "core/AppSettings.h"

class QListWidget;
class QListWidgetItem;
class GalleryModel;

// Simple grid/list of past captures (photos and videos) with thumbnail
// preview, "open" (default OS viewer), "delete", and "email to teacher"
// (photos only) actions.
class GalleryDialog : public QDialog {
    Q_OBJECT

public:
    GalleryDialog(GalleryModel *model, AppSettings *settings, QWidget *parent = nullptr);

private slots:
    void onOpenSelected();
    void onDeleteSelected();
    void onEmailSelected();
    void onEditSelected();
    void onCompareRequested();
    void onExportPdfRequested();
    void onTrashRequested();
    void onExportFolderRequested();
    void onSelectionChanged();
    void reloadItems();

private:
    GalleryModel *m_model;
    AppSettings *m_settings;
    QListWidget *m_listWidget;
    class QPushButton *m_emailButton;
    class QPushButton *m_editButton;
};
