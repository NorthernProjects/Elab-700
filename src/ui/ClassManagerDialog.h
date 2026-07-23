#pragma once

#include <QDialog>

#include "core/AppSettings.h"

class QLineEdit;
class QListWidget;

// Teacher-only editor (opened from the teacher panel) for the roster of
// classes: each with a name, the teacher's email (used by the gallery's
// "envoyer par courriel" button), and up to kMaxGroupsPerClass groups. Saves
// back to AppSettings on accept.
class ClassManagerDialog : public QDialog {
    Q_OBJECT

public:
    explicit ClassManagerDialog(AppSettings *settings, QWidget *parent = nullptr);

private slots:
    void onAddClass();
    void onRemoveClass();
    void onClassSelectionChanged();
    void onAddGroup();
    void onRemoveGroup();
    void onSave();

private:
    void loadClassIntoEditor(int index);
    void storeEditorIntoClass(int index);

    AppSettings *m_settings;
    QVector<SchoolClass> m_classes;
    int m_currentIndex = -1;

    QListWidget *m_classList;
    QLineEdit *m_nameEdit;
    QLineEdit *m_teacherNameEdit;
    QLineEdit *m_emailEdit;
    QLineEdit *m_passwordEdit;
    QListWidget *m_groupList;
};
