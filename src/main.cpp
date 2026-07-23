#include <QApplication>
#include <QFile>
#include <QIcon>
#include <QSettings>
#include <QTextStream>

#include "core/AppSettings.h"
#include "ui/MainWindow.h"
#include "ui/ModeSelectionDialog.h"
#include "ui/SplashScreen.h"

namespace {
QString loadThemeStylesheet(bool light)
{
    QFile file(light ? QStringLiteral(":/theme/light.qss") : QStringLiteral(":/theme/dark.qss"));
    if (!file.open(QFile::ReadOnly | QFile::Text))
        return {};
    QTextStream stream(&file);
    return stream.readAll();
}

// Earlier releases of the school edition stored their settings under the
// "RuelleDeLAvenir" organization; the unified single-exe app uses
// "ELab700Community" for everyone. On a PC that ran the old school build
// (this school's classroom machines), silently carry over any setting the
// new scope doesn't have yet — most importantly the classes/groups roster
// and the teacher PIN — so upgrading doesn't wipe the teacher's setup.
// One-shot, and never overwrites anything already set in the new scope.
void migrateLegacySchoolSettings()
{
    QSettings newSettings;
    if (newSettings.value(QStringLiteral("migration/fromSchoolDone"), false).toBool())
        return;

    QSettings legacy(QStringLiteral("RuelleDeLAvenir"), QStringLiteral("E-Lab 700"));
    const QStringList legacyKeys = legacy.allKeys();
    for (const QString &key : legacyKeys) {
        if (!newSettings.contains(key))
            newSettings.setValue(key, legacy.value(key));
    }
    newSettings.setValue(QStringLiteral("migration/fromSchoolDone"), true);
}
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("ELab700Community"));
    QApplication::setApplicationName(QStringLiteral("E-Lab 700"));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/branding/icon.png")));

    migrateLegacySchoolSettings();

    // Just to read the persisted light/dark preference before MainWindow (and
    // its own AppSettings instance) exists — both instances are thin QSettings
    // wrappers with no meaningful separate state, so this is harmless.
    AppSettings initialSettingsPeek;
    app.setStyleSheet(loadThemeStylesheet(initialSettingsPeek.lightTheme()));

    // First launch: pick the edition (Scolaire / Grand public / Laboratoire)
    // BEFORE the splash exists — the always-on-top splash would hide a modal
    // dialog behind it and the app would look frozen (see SplashScreen).
    if (initialSettingsPeek.appMode().isEmpty()) {
        ModeSelectionDialog chooser;
        if (chooser.exec() == QDialog::Accepted)
            initialSettingsPeek.setAppMode(chooser.selectedMode());
        else
            initialSettingsPeek.setAppMode(QStringLiteral("public")); // closed without choosing: sensible default
    }

    // The splash stays on top and visible while MainWindow constructs and
    // shows itself behind it (camera setup, etc.), then plays its animation
    // and closes — this hides the plain white flash a fresh QMainWindow
    // otherwise shows for a frame or two before its stylesheet/first paint.
    SplashScreen splash;
    splash.show();
    splash.startAnimating();
    app.processEvents();

    MainWindow window;
    window.show();
    app.processEvents();

    splash.playIntro();

    return app.exec();
}
