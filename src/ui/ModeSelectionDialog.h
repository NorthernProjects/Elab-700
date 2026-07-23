#pragma once

#include <QDialog>
#include <QString>

// First-launch (and re-invocable) edition chooser: one executable, the user
// picks which of the three free editions fits them — Scolaire, Grand public,
// or Laboratoire. Picking one applies that edition's feature defaults (see
// AppSettings::setAppMode); everything remains adjustable afterwards in the
// settings panel. Shown BEFORE the splash screen in main() — the splash is
// always-on-top and would hide a modal dialog behind it (see SplashScreen's
// class comment for the freeze that causes).
class ModeSelectionDialog : public QDialog {
    Q_OBJECT

public:
    explicit ModeSelectionDialog(QWidget *parent = nullptr);

    // "school", "public", or "lab" — valid once exec() returned Accepted.
    QString selectedMode() const { return m_selectedMode; }

private:
    void addChoice(const QString &mode, const QString &title, const QString &description);

    QString m_selectedMode;
};
