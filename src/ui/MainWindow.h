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
class QParallelAnimationGroup;

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
    // Animates the side panel's width (and fade) between its full width and
    // 0 — collapsing it out of the way or bringing it back. animate=false
    // jumps straight to the target state (used for immersive mode, which is
    // an instant full-screen switch, not a user-initiated collapse/expand).
    void setMicroscopeInfoPanelCollapsed(bool collapsed, bool animate = true);
    void promptRenamePhoto(const QString &path);
    void setImmersiveMode(bool immersive);
    void updateTimeLapseTimer();
    void startResolutionAutoTuning();
    void probeNextResolution();
    void updateLabTimerDisplay();

    double m_zoomFactor = 1.0;
    bool m_immersiveMode = false;
    bool m_timeLapseCapturing = false;
    // Set once the user manually picks a resolution from the top-bar menu:
    // onCameraConnected() then re-applies m_userChosenResolution on every
    // later reconnect within this session instead of running
    // startResolutionAutoTuning() and silently overriding their choice.
    bool m_userChoseResolution = false;
    QSize m_userChosenResolution;
    // Auto-tuning cycles setResolution() through several candidates right
    // after connect (see startResolutionAutoTuning) — reconfiguring a
    // DirectShow graph's format repeatedly in quick succession right after
    // opening it turned out to be unstable on some drivers (crashes on
    // camera select). Only ever run it once per app session; later
    // reconnects just keep whatever resolution the camera opens with.
    bool m_autoTuningDoneOnce = false;
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
    // Real layout sibling of m_videoView (not a floating overlay) so the
    // panel sits beside the video instead of on top of it — see
    // setMicroscopeInfoPanelCollapsed().
    QWidget *m_microscopeInfoContainer;
    QParallelAnimationGroup *m_microscopeInfoAnim = nullptr;
    bool m_microscopeInfoCollapsed = false;
    bool m_microscopeInfoCollapsedBeforeImmersive = false;
    int m_microscopeInfoExpandedWidth = 0;
};
