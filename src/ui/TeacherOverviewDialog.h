#pragma once

#include <QDialog>

#include "core/AppSettings.h"

class QTreeWidget;
class QTreeWidgetItem;

// Read-only cross-group view for the teacher: how many photos/videos each
// group of each class has captured, with a shortcut to open any group's
// gallery directly — without needing to log out and back in as that group,
// which is otherwise the only way to see another group's captures.
class TeacherOverviewDialog : public QDialog {
    Q_OBJECT

public:
    explicit TeacherOverviewDialog(AppSettings *settings, QWidget *parent = nullptr);

private slots:
    void onItemDoubleClicked(QTreeWidgetItem *item, int column);
    void onOpenSelectedGallery();
    void reloadCounts();

private:
    AppSettings *m_settings;
    QTreeWidget *m_tree;
};
