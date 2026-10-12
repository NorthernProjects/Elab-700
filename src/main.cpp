#include <QApplication>
#include <QFile>
#include <QIcon>
#include <QSettings>
#include <QTextStream>
#include <QTimer>
#include <QTranslator>
#include <QtGlobal>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QStringConverter>
#endif

#include "core/AppSettings.h"
#include "core/DirectShowInfo.h"
#include "core/NullCameraBackend.h"
#include "ui/MainWindow.h"
#include "ui/ModeSelectionDialog.h"
#include "ui/SplashScreen.h"
#include "ui/TeacherPanel.h"

namespace {
QString loadThemeStylesheet(bool light)
{
    QFile file(light ? QStringLiteral(":/theme/light.qss") : QStringLiteral(":/theme/dark.qss"));
    if (!file.open(QFile::ReadOnly | QFile::Text))
        return {};
    // Explicit UTF-8 (QTextStream's default codec differs between Qt 5 and 6).
    return QString::fromUtf8(file.readAll());
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
    // Diagnostic, no window: `E-Lab700.exe --list-cameras result.txt` writes
    // what Windows/DirectShow reports for every camera (name, USB path, real
    // supported resolutions) and exits. Read-only — never starts a stream.
    if (argc >= 3 && QString::fromLocal8Bit(argv[1]) == QLatin1String("--list-cameras")) {
        QFile out(QString::fromLocal8Bit(argv[2]));
        if (!out.open(QFile::WriteOnly | QFile::Text))
            return 1;
        QTextStream stream(&out);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
        stream.setEncoding(QStringConverter::Utf8);
#else
        stream.setCodec("UTF-8");
#endif
        const QVector<DirectShowDevice> devices = enumerateDirectShowDevices();
        stream << "cameras: " << devices.size() << "\n";
        for (const DirectShowDevice &device : devices) {
            stream << "\n[" << device.index << "] " << device.name << "\n  path: " << device.devicePath << "\n  modes:";
            for (const QSize &size : device.resolutions)
                stream << ' ' << size.width() << 'x' << size.height();
            stream << "\n";

            DirectShowControls controls(device.index);
            const struct {
                DsControl control;
                const char *label;
            } kControls[] = {{DsControl::Exposure, "exposure"}, {DsControl::Gain, "gain"},
                             {DsControl::Brightness, "brightness"}, {DsControl::WhiteBalance, "whiteBalance"}};
            for (const auto &entry : kControls) {
                const DsControlRange range = controls.range(entry.control);
                stream << "  " << entry.label << ": ";
                if (!range.available) {
                    stream << "not available\n";
                    continue;
                }
                long value = 0;
                bool isAuto = false;
                controls.read(entry.control, value, isAuto);
                stream << range.min << ".." << range.max << " step " << range.step << " default "
                       << range.defaultValue << " now " << value << (isAuto ? " (auto)" : " (manual)")
                       << (range.autoSupported ? ", auto supported" : "") << "\n";
            }
        }
        return 0;
    }

    // Visual check without showing anything on a real screen: `--grab out.png`
    // runs the whole app on Qt's offscreen platform, saves a picture of the
    // main window a few seconds after start-up and exits.
    // `--grab-teacher out.png` does the same for the teacher panel (no camera).
    QString grabPath;
    QString grabTeacherPath;
    for (int i = 1; i + 1 < argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg == QLatin1String("--grab"))
            grabPath = QString::fromLocal8Bit(argv[i + 1]);
        else if (arg == QLatin1String("--grab-teacher"))
            grabTeacherPath = QString::fromLocal8Bit(argv[i + 1]);
    }
    if (!grabPath.isEmpty() || !grabTeacherPath.isEmpty())
        qputenv("QT_QPA_PLATFORM", "offscreen");

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

    // Must be installed before any tr() call runs (i.e. before any widget is
    // constructed below) — see AppSettings::uiLanguage(). Absent in the
    // legacy Qt5/Windows 7 build (no :/i18n resource there), where load()
    // just fails harmlessly and the app stays French.
    QTranslator translator;
    if (initialSettingsPeek.uiLanguage() == QLatin1String("en")
        && translator.load(QStringLiteral(":/i18n/elab700_en.qm"))) {
        app.installTranslator(&translator);
    }

    if (!grabTeacherPath.isEmpty()) {
        NullCameraBackend noCamera;
        TeacherPanel panel(&initialSettingsPeek, &noCamera, []() { return QImage(); });
        panel.resize(1100, 2600); // tall enough that nothing needs scrolling
        panel.show();
        app.processEvents();
        panel.grab().save(grabTeacherPath);
        return 0;
    }

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

    if (!grabPath.isEmpty()) {
        QTimer::singleShot(5000, &window, [&window, &app, grabPath]() {
            window.grab().save(grabPath);
            app.quit();
        });
    }

    return app.exec();
}
