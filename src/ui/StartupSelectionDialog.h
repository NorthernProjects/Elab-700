#pragma once

#include <QDialog>

#include "core/AppSettings.h"

class QComboBox;
class QLabel;
class QLineEdit;

// Small popup opened by clicking the "Se connecter" / group area in the top
// bar (see TopStatusBar): pick a class and group, enter that class's shared
// password, and get in. Not real per-student authentication — just enough
// to stop a student picking someone else's group by accident, so photos and
// videos end up in the right group's gallery folder.
class StartupSelectionDialog : public QDialog {
    Q_OBJECT

public:
    explicit StartupSelectionDialog(const QVector<SchoolClass> &classes, QWidget *parent = nullptr);

    QString selectedClassName() const;
    QString selectedGroupName() const;

private slots:
    void onClassChanged(int index);
    void onConnectClicked();

private:
    QVector<SchoolClass> m_classes;
    QComboBox *m_classCombo;
    QComboBox *m_groupCombo;
    QLineEdit *m_passwordEdit;
    QLabel *m_errorLabel;
};
