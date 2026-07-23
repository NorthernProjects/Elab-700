#include "AppSettings.h"

#include <QCryptographicHash>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QStandardPaths>

namespace {
constexpr auto kCaptureFolderKey = "capture/folder";
constexpr auto kStudentLockedKey = "student/locked";
constexpr auto kTeacherPinHashKey = "teacher/pinHash";
constexpr auto kIdleTimeoutKey = "idle/timeoutMinutes";
constexpr auto kMonochromeKey = "display/monochrome";
constexpr auto kLightThemeKey = "display/lightTheme";
constexpr auto kClassesKey = "classes/json";
constexpr auto kShowGridKey = "display/showGrid";
constexpr auto kShowFocusIndicatorKey = "display/showFocusIndicator";
constexpr auto kTimeLapseEnabledKey = "timelapse/enabled";
constexpr auto kTimeLapseIntervalKey = "timelapse/intervalSeconds";
constexpr auto kShowScaleBarKey = "scalebar/show";
constexpr auto kScaleBarCalibrationKey = "scalebar/micronsPer100Px";
constexpr auto kMicroscopeNameKey = "microscope/name";
constexpr auto kPhotoFormatKey = "capture/photoFormat";
constexpr auto kSaveCaptureMetadataKey = "capture/saveMetadata";
constexpr auto kAppModeKey = "app/mode";
constexpr auto kFeatureClassesKey = "features/classes";
constexpr auto kFeaturePinLockKey = "features/pinLock";
constexpr auto kFeatureLabTimerKey = "features/labTimer";
constexpr auto kFeatureLearningAidsKey = "features/learningAids";
constexpr auto kFeatureLabToolsKey = "features/labTools";
constexpr auto kAutoBackupEnabledKey = "autobackup/enabled";
constexpr auto kAutoBackupDestinationKey = "autobackup/destination";
constexpr auto kAutoBackupIntervalDaysKey = "autobackup/intervalDays";
constexpr auto kLastAutoBackupAtKey = "autobackup/lastBackupAt";
constexpr int kDefaultTimeLapseIntervalSeconds = 30;
constexpr int kDefaultAutoBackupIntervalDays = 7;
// Rough placeholder so the scale bar shows something reasonable out of the
// box instead of requiring calibration before it's useful at all — not a
// real measurement of this specific microscope/objective combination. The
// teacher should recalibrate it (Mode professeur > Affichage) against an
// actual reference (a stage micrometer slide, a ruler, anything of known
// size) if the class needs the scale bar to be trustworthy.
constexpr double kDefaultScaleBarMicronsPer100Px = 500.0;
constexpr auto kDefaultPin = "1234"; // documented in README; teacher should change it on first use
constexpr int kDefaultIdleTimeoutMinutes = 5;

// Windows forbids these in folder names; capture subfolders are built from
// teacher-entered class/group names so this can't be skipped.
QString sanitizeForFolderName(const QString &name)
{
    QString result = name.trimmed();
    for (const QChar &forbidden : QStringLiteral("\\/:*?\"<>|"))
        result.replace(forbidden, QLatin1Char('_'));
    return result.isEmpty() ? QStringLiteral("_") : result;
}
}

AppSettings::AppSettings(QObject *parent) : QObject(parent)
{
    QSettings settings;
    if (!settings.contains(kCaptureFolderKey))
        settings.setValue(kCaptureFolderKey, defaultCaptureFolder());
#ifdef E_LAB_SCHOOL_BRANDING
    // Only this school's own private build pre-seeds a working default PIN
    // (documented in README, already relied on at the school) — the public
    // open-source build never activates PIN protection until the user
    // deliberately creates their own via Fonctionnalités > Code PIN, so a
    // stranger's install is never gated by a PIN they never chose.
    if (!settings.contains(kTeacherPinHashKey))
        setTeacherPin(kDefaultPin);
#endif
}

QString AppSettings::defaultCaptureFolder()
{
    const QString picturesDir = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    const QString path = QDir(picturesDir).filePath("E-Lab 700");
    QDir().mkpath(path);
    return path;
}

QString AppSettings::captureFolder() const
{
    QSettings settings;
    return settings.value(kCaptureFolderKey, defaultCaptureFolder()).toString();
}

void AppSettings::setCaptureFolder(const QString &path)
{
    QDir().mkpath(path);
    QSettings settings;
    settings.setValue(kCaptureFolderKey, path);
    emit captureFolderChanged(path);
}

bool AppSettings::studentModeLocked() const
{
    QSettings settings;
    return settings.value(kStudentLockedKey, false).toBool();
}

void AppSettings::setStudentModeLocked(bool locked)
{
    QSettings settings;
    settings.setValue(kStudentLockedKey, locked);
    emit studentModeLockedChanged(locked);
}

QString AppSettings::teacherPinHash() const
{
    QSettings settings;
    return settings.value(kTeacherPinHashKey).toString();
}

void AppSettings::setTeacherPin(const QString &pin)
{
    const QByteArray hash = QCryptographicHash::hash(pin.toUtf8(), QCryptographicHash::Sha256).toHex();
    QSettings settings;
    settings.setValue(kTeacherPinHashKey, QString::fromLatin1(hash));
}

bool AppSettings::checkTeacherPin(const QString &pin) const
{
    const QByteArray hash = QCryptographicHash::hash(pin.toUtf8(), QCryptographicHash::Sha256).toHex();
    return QString::fromLatin1(hash) == teacherPinHash();
}

int AppSettings::idleTimeoutMinutes() const
{
    QSettings settings;
    return settings.value(kIdleTimeoutKey, kDefaultIdleTimeoutMinutes).toInt();
}

void AppSettings::setIdleTimeoutMinutes(int minutes)
{
    QSettings settings;
    settings.setValue(kIdleTimeoutKey, minutes);
    emit idleTimeoutMinutesChanged(minutes);
}

bool AppSettings::monochromeDisplay() const
{
    QSettings settings;
    return settings.value(kMonochromeKey, false).toBool();
}

void AppSettings::setMonochromeDisplay(bool enabled)
{
    QSettings settings;
    settings.setValue(kMonochromeKey, enabled);
    emit monochromeDisplayChanged(enabled);
}

bool AppSettings::lightTheme() const
{
    QSettings settings;
    return settings.value(kLightThemeKey, false).toBool();
}

void AppSettings::setLightTheme(bool enabled)
{
    QSettings settings;
    settings.setValue(kLightThemeKey, enabled);
    emit lightThemeChanged(enabled);
}

bool AppSettings::showGrid() const
{
    QSettings settings;
    return settings.value(kShowGridKey, false).toBool();
}

void AppSettings::setShowGrid(bool enabled)
{
    QSettings settings;
    settings.setValue(kShowGridKey, enabled);
    emit showGridChanged(enabled);
}

bool AppSettings::showFocusIndicator() const
{
    QSettings settings;
    return settings.value(kShowFocusIndicatorKey, true).toBool();
}

void AppSettings::setShowFocusIndicator(bool enabled)
{
    QSettings settings;
    settings.setValue(kShowFocusIndicatorKey, enabled);
    emit showFocusIndicatorChanged(enabled);
}

bool AppSettings::timeLapseEnabled() const
{
    QSettings settings;
    return settings.value(kTimeLapseEnabledKey, false).toBool();
}

void AppSettings::setTimeLapseEnabled(bool enabled)
{
    QSettings settings;
    settings.setValue(kTimeLapseEnabledKey, enabled);
    emit timeLapseEnabledChanged(enabled);
}

int AppSettings::timeLapseIntervalSeconds() const
{
    QSettings settings;
    return settings.value(kTimeLapseIntervalKey, kDefaultTimeLapseIntervalSeconds).toInt();
}

void AppSettings::setTimeLapseIntervalSeconds(int seconds)
{
    QSettings settings;
    settings.setValue(kTimeLapseIntervalKey, seconds);
    emit timeLapseIntervalSecondsChanged(seconds);
}

bool AppSettings::showScaleBar() const
{
    QSettings settings;
    return settings.value(kShowScaleBarKey, true).toBool();
}

void AppSettings::setShowScaleBar(bool enabled)
{
    QSettings settings;
    settings.setValue(kShowScaleBarKey, enabled);
    emit showScaleBarChanged(enabled);
}

double AppSettings::scaleBarMicronsPer100Px() const
{
    QSettings settings;
    return settings.value(kScaleBarCalibrationKey, kDefaultScaleBarMicronsPer100Px).toDouble();
}

void AppSettings::setScaleBarMicronsPer100Px(double value)
{
    QSettings settings;
    settings.setValue(kScaleBarCalibrationKey, value);
    emit scaleBarMicronsPer100PxChanged(value);
}

bool AppSettings::autoBackupEnabled() const
{
    QSettings settings;
    return settings.value(kAutoBackupEnabledKey, false).toBool();
}

void AppSettings::setAutoBackupEnabled(bool enabled)
{
    QSettings settings;
    settings.setValue(kAutoBackupEnabledKey, enabled);
    emit autoBackupEnabledChanged(enabled);
}

QString AppSettings::autoBackupDestination() const
{
    QSettings settings;
    return settings.value(kAutoBackupDestinationKey).toString();
}

void AppSettings::setAutoBackupDestination(const QString &path)
{
    QSettings settings;
    settings.setValue(kAutoBackupDestinationKey, path);
    emit autoBackupDestinationChanged(path);
}

int AppSettings::autoBackupIntervalDays() const
{
    QSettings settings;
    return settings.value(kAutoBackupIntervalDaysKey, kDefaultAutoBackupIntervalDays).toInt();
}

void AppSettings::setAutoBackupIntervalDays(int days)
{
    QSettings settings;
    settings.setValue(kAutoBackupIntervalDaysKey, days);
    emit autoBackupIntervalDaysChanged(days);
}

QDateTime AppSettings::lastAutoBackupAt() const
{
    QSettings settings;
    return settings.value(kLastAutoBackupAtKey).toDateTime();
}

void AppSettings::setLastAutoBackupAt(const QDateTime &when)
{
    QSettings settings;
    settings.setValue(kLastAutoBackupAtKey, when);
}

QString AppSettings::microscopeName() const
{
    QSettings settings;
    return settings.value(kMicroscopeNameKey).toString().trimmed();
}

void AppSettings::setMicroscopeName(const QString &name)
{
    QSettings settings;
    settings.setValue(kMicroscopeNameKey, name.trimmed());
    emit microscopeNameChanged(name.trimmed());
}

QString AppSettings::photoFormat() const
{
    QSettings settings;
    const QString format = settings.value(kPhotoFormatKey, QStringLiteral("png")).toString();
    // Whitelist: this value ends up as a file extension handed to
    // cv::imwrite, so never let an arbitrary registry string through.
    if (format == QLatin1String("jpg") || format == QLatin1String("tiff"))
        return format;
    return QStringLiteral("png");
}

void AppSettings::setPhotoFormat(const QString &format)
{
    QSettings settings;
    settings.setValue(kPhotoFormatKey, format);
    emit photoFormatChanged(format);
}

QString AppSettings::appMode() const
{
    QSettings settings;
    const QString mode = settings.value(kAppModeKey).toString();
    if (mode == QLatin1String("school") || mode == QLatin1String("public") || mode == QLatin1String("lab"))
        return mode;
    return {};
}

void AppSettings::setAppMode(const QString &mode)
{
    QSettings settings;
    settings.setValue(kAppModeKey, mode);

    // Choosing an edition (re)applies that edition's feature defaults — the
    // user can then re-toggle any individual feature in the settings panel
    // ("Fonctionnalités supplémentaires") without changing edition.
    const bool school = (mode == QLatin1String("school"));
    const bool lab = (mode == QLatin1String("lab"));
    settings.setValue(kFeatureClassesKey, school);
    settings.setValue(kFeaturePinLockKey, school);
    settings.setValue(kFeatureLabTimerKey, school);
    settings.setValue(kFeatureLearningAidsKey, !lab);
    settings.setValue(kFeatureLabToolsKey, lab);

    emit appModeChanged(mode);
    emit featureFlagsChanged();
}

bool AppSettings::featureClassesEnabled() const
{
    QSettings settings;
    return settings.value(kFeatureClassesKey, false).toBool();
}

void AppSettings::setFeatureClassesEnabled(bool enabled)
{
    QSettings settings;
    settings.setValue(kFeatureClassesKey, enabled);
    emit featureFlagsChanged();
}

bool AppSettings::featurePinLockEnabled() const
{
    QSettings settings;
    return settings.value(kFeaturePinLockKey, false).toBool();
}

void AppSettings::setFeaturePinLockEnabled(bool enabled)
{
    QSettings settings;
    settings.setValue(kFeaturePinLockKey, enabled);
    emit featureFlagsChanged();
}

bool AppSettings::featureLabTimerEnabled() const
{
    QSettings settings;
    return settings.value(kFeatureLabTimerKey, false).toBool();
}

void AppSettings::setFeatureLabTimerEnabled(bool enabled)
{
    QSettings settings;
    settings.setValue(kFeatureLabTimerKey, enabled);
    emit featureFlagsChanged();
}

bool AppSettings::featureLearningAidsEnabled() const
{
    QSettings settings;
    return settings.value(kFeatureLearningAidsKey, true).toBool();
}

void AppSettings::setFeatureLearningAidsEnabled(bool enabled)
{
    QSettings settings;
    settings.setValue(kFeatureLearningAidsKey, enabled);
    emit featureFlagsChanged();
}

bool AppSettings::featureLabToolsEnabled() const
{
    QSettings settings;
    return settings.value(kFeatureLabToolsKey, false).toBool();
}

void AppSettings::setFeatureLabToolsEnabled(bool enabled)
{
    QSettings settings;
    settings.setValue(kFeatureLabToolsKey, enabled);
    emit featureFlagsChanged();
}

bool AppSettings::saveCaptureMetadata() const
{
    QSettings settings;
    return settings.value(kSaveCaptureMetadataKey, false).toBool();
}

void AppSettings::setSaveCaptureMetadata(bool enabled)
{
    QSettings settings;
    settings.setValue(kSaveCaptureMetadataKey, enabled);
    emit saveCaptureMetadataChanged(enabled);
}

QVector<SchoolClass> AppSettings::classes() const
{
    QSettings settings;
    const QByteArray json = settings.value(kClassesKey).toByteArray();
    const QJsonArray array = QJsonDocument::fromJson(json).array();

    QVector<SchoolClass> result;
    result.reserve(array.size());
    for (const QJsonValue &value : array) {
        const QJsonObject obj = value.toObject();
        SchoolClass schoolClass;
        schoolClass.name = obj.value(QStringLiteral("name")).toString();
        schoolClass.teacherName = obj.value(QStringLiteral("teacherName")).toString();
        schoolClass.teacherEmail = obj.value(QStringLiteral("teacherEmail")).toString();
        schoolClass.password = obj.value(QStringLiteral("password")).toString();
        for (const QJsonValue &group : obj.value(QStringLiteral("groups")).toArray())
            schoolClass.groups.append(group.toString());
        result.append(schoolClass);
    }
    return result;
}

void AppSettings::setClasses(const QVector<SchoolClass> &classes)
{
    QJsonArray array;
    for (const SchoolClass &schoolClass : classes) {
        QJsonObject obj;
        obj.insert(QStringLiteral("name"), schoolClass.name);
        obj.insert(QStringLiteral("teacherName"), schoolClass.teacherName);
        obj.insert(QStringLiteral("teacherEmail"), schoolClass.teacherEmail);
        obj.insert(QStringLiteral("password"), schoolClass.password);
        obj.insert(QStringLiteral("groups"), QJsonArray::fromStringList(schoolClass.groups));
        array.append(obj);
    }

    QSettings settings;
    settings.setValue(kClassesKey, QJsonDocument(array).toJson(QJsonDocument::Compact));
}

QString AppSettings::currentTeacherName() const
{
    for (const SchoolClass &schoolClass : classes()) {
        if (schoolClass.name == m_currentClassName)
            return schoolClass.teacherName;
    }
    return {};
}

QString AppSettings::currentTeacherEmail() const
{
    for (const SchoolClass &schoolClass : classes()) {
        if (schoolClass.name == m_currentClassName)
            return schoolClass.teacherEmail;
    }
    return {};
}

QString AppSettings::activeCaptureFolder() const
{
    if (m_currentClassName.isEmpty() || m_currentGroupName.isEmpty())
        return captureFolder();

    return groupCaptureFolder(m_currentClassName, m_currentGroupName);
}

QString AppSettings::groupCaptureFolder(const QString &className, const QString &groupName) const
{
    return QDir(captureFolder()).filePath(
        sanitizeForFolderName(className) + QLatin1Char('/') + sanitizeForFolderName(groupName));
}

void AppSettings::setActiveGroup(const QString &className, const QString &groupName)
{
    m_currentClassName = className;
    m_currentGroupName = groupName;

    const QString folder = activeCaptureFolder();
    QDir().mkpath(folder);
    emit activeCaptureFolderChanged(folder);
}
