#pragma once

#include <QMainWindow>
#include <QScopedPointer>
#include <QSize>
#include <QTimer>
#include <QVector>

#include "core/AppSettings.h"
#include "core/CameraManager.h"
#include "core/CaptureManager.h"
#include "core/GalleryModel.h"

class BottomBar;
class IdleScreen;
class MicroscopeInfoPanel;
class TopStatusBar;
class VideoView;

// Student-facing main window: nearly full-screen video, a bottom action bar,
// a slim top status strip, and a discreet gear button that opens the
// PIN-gated teacher panel. No technical menus are shown here by design.
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onPhotoRequested();
    void onVideoToggleRequested();
    void onAutoRequested();
    void onGalleryRequested();
    void onFullscreenRequested();
    void onTeacherModeRequested();

    void onCameraConnected(const CameraDeviceInfo &info);
    void onCameraDisconnected();
    void onZoomInRequested();
    void onZoomOutRequested();
    void onZoomResetRequested();
    void onGroupSelectionRequested();
    void onMicroscopeInfoRequested();
    void onResolutionClicked();
    void onConnectionClicked();
    void onVideoDoubleClicked();
    void showIdleScreen();
    void onTimeLapseTick();
    void onLabTimerTick();
    void startLabTimer(int minutes);
    void stopLabTimer();
    void checkAutoBackupDue();

private:
    bool confirmTeacherPin();
    void restartIdleTimer();
    void positionMicroscopeInfoPanel();
    void promptRenamePhoto(const QString &path);
    void setImmersiveMode(bool immersive);
    void updateTimeLapseTimer();
    void startResolutionAutoTuning();
    void probeNextResolution();
    void updateLabTimerDisplay();

    double m_zoomFactor = 1.0;
    bool m_immersiveMode = false;
    bool m_timeLapseCapturing = false;
    double m_smoothedFocusScore = 0.0;
    double m_lastReportedFps = 0.0;
    QVector<QSize> m_resolutionProbeCandidates;
    int m_resolutionProbeIndex = 0;
    int m_labSecondsRemaining = 0;
    AppSettings m_settings;
    CameraManager m_cameraManager;
    CaptureManager m_captureManager;
    GalleryModel m_galleryModel;
    QTimer m_idleTimer;
    QTimer m_timeLapseTimer;
    QTimer m_labCountdownTimer;
    QTimer m_autoBackupCheckTimer;

    TopStatusBar *m_topBar;
    VideoView *m_videoView;
    BottomBar *m_bottomBar;
    IdleScreen *m_idleScreen;
    MicroscopeInfoPanel *m_microscopeInfoPanel;
};
