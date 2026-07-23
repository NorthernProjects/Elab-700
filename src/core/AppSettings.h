#pragma once

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

// One class (e.g. "Secondaire 1A") with its teacher's email (destination for
// the gallery's "envoyer par courriel" button), a shared class password
// (the same for every student in that class — just enough to stop someone
// picking another class/group by accident, not real per-student auth), and
// up to a handful of named groups; each group gets its own capture
// subfolder so students only ever see their own group's photos/videos.
struct SchoolClass {
    QString name;
    QString teacherName;
    QString teacherEmail;
    QString password;
    QStringList groups;
};

// Thin QSettings wrapper for the handful of persistent preferences the app
// needs. Kept separate from CaptureManager/CameraManager so the UI layer can
// read/write settings without depending on capture or camera internals.
class AppSettings : public QObject {
    Q_OBJECT

public:
    explicit AppSettings(QObject *parent = nullptr);

    QString captureFolder() const;
    void setCaptureFolder(const QString &path);

    bool studentModeLocked() const;
    void setStudentModeLocked(bool locked);

    QString teacherPinHash() const;
    void setTeacherPin(const QString &pin);
    bool checkTeacherPin(const QString &pin) const;

    // 0 disables the idle screen entirely.
    int idleTimeoutMinutes() const;
    void setIdleTimeoutMinutes(int minutes);

    bool monochromeDisplay() const;
    void setMonochromeDisplay(bool enabled);

    bool lightTheme() const;
    void setLightTheme(bool enabled);

    // Framing grid (rule-of-thirds) overlay on the live video.
    bool showGrid() const;
    void setShowGrid(bool enabled);

    // Per-frame sharpness indicator (a rough focus aid, not a scientific
    // measurement — see VideoView's comment on how it's computed).
    bool showFocusIndicator() const;
    void setShowFocusIndicator(bool enabled);

    // Time-lapse: silently takes a photo on a fixed interval while enabled
    // and a camera is connected (no rename prompt — that's only for manual
    // captures, would be far too disruptive here).
    bool timeLapseEnabled() const;
    void setTimeLapseEnabled(bool enabled);
    int timeLapseIntervalSeconds() const;
    void setTimeLapseIntervalSeconds(int seconds);

    // On-screen scale bar. Calibration is teacher-entered: micrometers
    // corresponding to 100 pixels of the camera's native captured frame at
    // whatever resolution was active when they calibrated it (changing
    // resolution afterwards means recalibrating) — deliberately simple,
    // not a precision instrument.
    bool showScaleBar() const;
    void setShowScaleBar(bool enabled);
    double scaleBarMicronsPer100Px() const;
    void setScaleBarMicronsPer100Px(double value);

    // User-entered microscope model name (public/open-source builds: shown
    // in the top bar and the info panel instead of a hardcoded model).
    // Empty = not set, callers fall back to a generic label.
    QString microscopeName() const;
    void setMicroscopeName(const QString &name);

    // Photo file format: "png" (default, lossless), "jpg", or "tiff"
    // (lab-friendly). Getter whitelists the value since it becomes a file
    // extension passed to the image encoder.
    QString photoFormat() const;
    void setPhotoFormat(const QString &format);

    // Write a small .txt sidecar next to every photo with the capture
    // conditions (date, resolution, exposure, gain, scale calibration...) —
    // lab-oriented, off by default.
    bool saveCaptureMetadata() const;
    void setSaveCaptureMetadata(bool enabled);

    // One executable, three runtime editions: "school" (élèves + professeur,
    // classes/groupes...), "public" (grand public, simple), "lab"
    // (laboratoire, outils pro). Empty = never chosen yet — the first-launch
    // chooser dialog runs. Choosing an edition applies its feature-flag
    // defaults below; each flag can then be re-toggled individually so any
    // edition can borrow another edition's features.
    QString appMode() const;
    void setAppMode(const QString &mode);

    bool featureClassesEnabled() const;        // classes/groupes + login + vue d'ensemble
    void setFeatureClassesEnabled(bool enabled);
    bool featurePinLockEnabled() const;        // PIN professeur + verrouillage élève
    void setFeaturePinLockEnabled(bool enabled);
    bool featureLabTimerEnabled() const;       // minuteur d'observation
    void setFeatureLabTimerEnabled(bool enabled);
    bool featureLearningAidsEnabled() const;   // schéma du microscope + glossaire
    void setFeatureLearningAidsEnabled(bool enabled);
    bool featureLabToolsEnabled() const;       // format photo TIFF/JPG + métadonnées
    void setFeatureLabToolsEnabled(bool enabled);

    // Persistent, teacher-configured roster (see "Gérer les classes et
    // groupes" in the teacher panel).
    QVector<SchoolClass> classes() const;
    void setClasses(const QVector<SchoolClass> &classes);

    // Periodic automatic backup of the whole capture folder (all classes/
    // groups) to a teacher-chosen destination — a background safety net
    // complementing the gallery's manual "Exporter le dossier" button.
    bool autoBackupEnabled() const;
    void setAutoBackupEnabled(bool enabled);
    QString autoBackupDestination() const;
    void setAutoBackupDestination(const QString &path);
    int autoBackupIntervalDays() const;
    void setAutoBackupIntervalDays(int days);
    QDateTime lastAutoBackupAt() const;
    void setLastAutoBackupAt(const QDateTime &when);

    // Current session only (chosen at startup, never persisted): which
    // class/group subfolder captures are currently written to.
    QString currentClassName() const { return m_currentClassName; }
    QString currentGroupName() const { return m_currentGroupName; }
    QString currentTeacherName() const;
    QString currentTeacherEmail() const;
    QString activeCaptureFolder() const;
    void setActiveGroup(const QString &className, const QString &groupName);

    // Same folder-path construction activeCaptureFolder() uses for whichever
    // group is currently logged in, but for any class/group pair — used by
    // the teacher overview to find each group's folder without requiring a
    // login switch.
    QString groupCaptureFolder(const QString &className, const QString &groupName) const;

signals:
    void captureFolderChanged(const QString &path);
    void studentModeLockedChanged(bool locked);
    void idleTimeoutMinutesChanged(int minutes);
    void monochromeDisplayChanged(bool enabled);
    void lightThemeChanged(bool enabled);
    void activeCaptureFolderChanged(const QString &path);
    void showGridChanged(bool enabled);
    void showFocusIndicatorChanged(bool enabled);
    void timeLapseEnabledChanged(bool enabled);
    void timeLapseIntervalSecondsChanged(int seconds);
    void showScaleBarChanged(bool enabled);
    void scaleBarMicronsPer100PxChanged(double value);
    void microscopeNameChanged(const QString &name);
    void photoFormatChanged(const QString &format);
    void saveCaptureMetadataChanged(bool enabled);
    void appModeChanged(const QString &mode);
    void featureFlagsChanged();
    void autoBackupEnabledChanged(bool enabled);
    void autoBackupDestinationChanged(const QString &path);
    void autoBackupIntervalDaysChanged(int days);

private:
    static QString defaultCaptureFolder();

    QString m_currentClassName;
    QString m_currentGroupName;
};
