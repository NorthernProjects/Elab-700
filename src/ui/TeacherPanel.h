#pragma once

#include <functional>

#include <QDialog>
#include <QImage>

#include "core/AppSettings.h"
#include "core/CameraBackend.h"

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QSlider;
class QSpinBox;

// Advanced settings dialog, reachable only via the PIN-gated gear button on
// the student screen. Exposes exposure, gain, white balance, resolution,
// save folder, the "lock student mode" switch, the teacher PIN, the
// idle-screen delay, and the monochrome display option. Scrollable: this
// panel keeps growing and must never silently clip its last row.
class TeacherPanel : public QDialog {
    Q_OBJECT

public:
    // frameProvider is called fresh each time "Étalonner..." is clicked
    // (rather than passing a single snapshot up front) so the calibration
    // wizard always opens with whatever's actually on screen at that
    // moment, even if the teacher panel has been open a while.
    TeacherPanel(AppSettings *settings, CameraBackend *backend, std::function<QImage()> frameProvider,
                 QWidget *parent = nullptr);

signals:
    void labTimerStartRequested(int minutes);
    void labTimerStopRequested();

private slots:
    void onAutoExposureToggled(bool checked);
    void onAutoWhiteBalanceToggled(bool checked);
    void onExposureSliderChanged(int value);
    void onGainSliderChanged(int value);
    void onResolutionChanged(int index);
    void onBrowseFolder();
    void onChangePin();
    void onIdleTimeoutChanged(int minutes);
    void onManageClasses();
    void onScaleBarCalibrationChanged(double value);
    void onOpenCalibrationWizard();
    void onOpenOverview();
    void onChooseBackupDestination();
    void onAutoBackupNow();

private:
    void refreshCameraInfo();
    void refreshAutoBackupLastLabel();

    AppSettings *m_settings;
    CameraBackend *m_backend;
    std::function<QImage()> m_frameProvider;

    QLabel *m_cameraInfoLabel;
    QCheckBox *m_autoExposureCheck;
    QSlider *m_exposureSlider;
    QLabel *m_exposureValueLabel;
    QCheckBox *m_autoWhiteBalanceCheck;
    QSlider *m_gainSlider;
    QLabel *m_gainValueLabel;
    QComboBox *m_resolutionCombo;
    QLineEdit *m_folderEdit;
    QCheckBox *m_studentLockCheck;
    QCheckBox *m_monochromeCheck;
    QCheckBox *m_lightThemeCheck;
    QCheckBox *m_gridCheck;
    QCheckBox *m_focusIndicatorCheck;
    QCheckBox *m_scaleBarCheck;
    QDoubleSpinBox *m_scaleBarCalibrationSpin;
    QCheckBox *m_timeLapseCheck;
    QSpinBox *m_timeLapseIntervalSpin;
    QLineEdit *m_newPinEdit;
    QLineEdit *m_confirmPinEdit;
    QSpinBox *m_idleTimeoutSpin;
    QSpinBox *m_labTimerMinutesSpin;
    QCheckBox *m_autoBackupCheck;
    QLineEdit *m_autoBackupDestEdit;
    QSpinBox *m_autoBackupIntervalSpin;
    QLabel *m_autoBackupLastLabel;
};
