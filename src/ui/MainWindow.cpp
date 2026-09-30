#include "MainWindow.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include <QAction>
#include <QApplication>
#include <QCloseEvent>
#include <QCursor>
#include <QDateTime>
#include <QDir>
#include <QEasingCurve>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QParallelAnimationGroup>
#include <QPixmap>
#include <QPropertyAnimation>
#include <QStatusBar>
#include <QTextStream>
#include <QVBoxLayout>

#include "BottomBar.h"
#include "GalleryDialog.h"
#include "AnalysisToolsDialog.h"
#include "HelpDialog.h"
#include "IdleScreen.h"
#include "MicroscopeInfoPanel.h"
#include "StartupSelectionDialog.h"
#include "TeacherPanel.h"
#include "TopStatusBar.h"
#include "VideoView.h"
#include "core/FileUtils.h"

#if defined(Q_OS_MAC)
#include "core/WindowChromeMac.h"
#endif

namespace {

constexpr double kMinZoom = 1.0;
constexpr double kMaxZoom = 4.0;
constexpr double kZoomStep = 0.25;

// Digital zoom only: crops the center of the frame then lets VideoView's
// existing scale-to-fit handle the rest. Doesn't and can't widen the
// camera's actual field of view, which is fixed by the microscope's optics.
QImage applyZoom(const QImage &image, double factor)
{
    if (factor <= kMinZoom || image.isNull())
        return image;

    const int croppedWidth = qMax(1, static_cast<int>(image.width() / factor));
    const int croppedHeight = qMax(1, static_cast<int>(image.height() / factor));
    const QRect rect((image.width() - croppedWidth) / 2, (image.height() - croppedHeight) / 2,
                      croppedWidth, croppedHeight);
    return image.copy(rect);
}

QImage applyMonochrome(const QImage &image, bool enabled)
{
    if (!enabled || image.isNull())
        return image;
    return image.convertToFormat(QImage::Format_Grayscale8);
}

// A rough, qualitative sharpness aid for the on-screen focus indicator, not
// a scientific measurement — downsamples first (see comment below) then
// scores a simple Laplacian variance, scaled heuristically into 0-100.
double computeSharpnessScore(const QImage &image)
{
    if (image.isNull())
        return 0.0;

    const QImage small = image.scaledToWidth(160, Qt::FastTransformation)
                              .convertToFormat(QImage::Format_Grayscale8);
    const int w = small.width();
    const int h = small.height();
    if (w < 3 || h < 3)
        return 0.0;

    double sum = 0.0;
    double sumSq = 0.0;
    int count = 0;
    for (int y = 1; y < h - 1; ++y) {
        const uchar *row = small.constScanLine(y);
        const uchar *rowUp = small.constScanLine(y - 1);
        const uchar *rowDown = small.constScanLine(y + 1);
        for (int x = 1; x < w - 1; ++x) {
            const int lap = 4 * row[x] - row[x - 1] - row[x + 1] - rowUp[x] - rowDown[x];
            sum += lap;
            sumSq += static_cast<double>(lap) * lap;
            ++count;
        }
    }
    if (count == 0)
        return 0.0;

    const double mean = sum / count;
    const double variance = (sumSq / count) - (mean * mean);
    // The multiplier (raw Laplacian variance is naturally small) was
    // originally tuned against typical low-magnification views; high-power
    // objectives tend to fill the frame with smoother, lower-contrast
    // detail even in sharp focus, which under-scored as "blurry" on the
    // same scale. Bumped up so genuinely sharp frames read as sharp across
    // objectives — this stays a rough visual aid, not a precise measurement.
    return qBound(0.0, std::sqrt(qMax(0.0, variance)) * 4.0, 100.0);
}

QString sanitizeForFileName(const QString &name)
{
    QString result = name.trimmed();
    for (const QChar &forbidden : QStringLiteral("\\/:*?\"<>|"))
        result.replace(forbidden, QLatin1Char('_'));
    return result;
}

void applyThemeStylesheet(bool light)
{
    QFile file(light ? QStringLiteral(":/theme/light.qss") : QStringLiteral(":/theme/dark.qss"));
    if (!file.open(QFile::ReadOnly | QFile::Text))
        return;
    qApp->setStyleSheet(QString::fromUtf8(file.readAll()));
}

} // namespace

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_captureManager(&m_settings, this)
{
    setWindowTitle(QStringLiteral("E-Lab 700"));
    resize(1280, 800);

    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_topBar = new TopStatusBar(central);
    m_videoView = new VideoView(central);
    m_bottomBar = new BottomBar(central);

    m_microscopeInfoPanel = new MicroscopeInfoPanel(central);

    // A real layout sibling of the video (not a floating overlay positioned
    // on top of it) so it takes up its own space beside the image instead
    // of covering part of it. Wrapped in a container so its width can be
    // animated down to 0 to collapse it — the panel's own drop shadow
    // effect means it can't also carry the container's opacity fade, hence
    // the separate widget.
    m_microscopeInfoContainer = new QWidget(central);
    m_microscopeInfoContainer->setObjectName("microscopeInfoContainer");
    auto *infoContainerLayout = new QVBoxLayout(m_microscopeInfoContainer);
    infoContainerLayout->setContentsMargins(16, 24, 24, 24);
    infoContainerLayout->addWidget(m_microscopeInfoPanel);
    infoContainerLayout->addStretch(1);

    // No opacity effect on the container: nesting a QGraphicsEffect on this
    // widget together with the panel's own drop-shadow effect (a second,
    // separate QGraphicsEffect one level down) made the whole card render
    // as blank/invisible — a real Qt limitation with nested graphics
    // effects, not a sizing bug. The width animation alone (see
    // setMicroscopeInfoPanelCollapsed) still reads as a clean slide.

    // Expanded by default ("affiché tout le temps") — pin min==max to its
    // natural content width so it starts fully shown, not mid-animation.
    m_microscopeInfoExpandedWidth = m_microscopeInfoContainer->sizeHint().width();
    m_microscopeInfoContainer->setMinimumWidth(m_microscopeInfoExpandedWidth);
    m_microscopeInfoContainer->setMaximumWidth(m_microscopeInfoExpandedWidth);

    auto *videoRow = new QHBoxLayout();
    videoRow->setContentsMargins(0, 0, 0, 0);
    videoRow->setSpacing(0);
    videoRow->addWidget(m_videoView, 1);
    videoRow->addWidget(m_microscopeInfoContainer, 0);

    layout->addWidget(m_topBar);
    layout->addLayout(videoRow, 1);
    layout->addWidget(m_bottomBar);

#if defined(Q_OS_MAC)
    // Our own top bar already shows the app logo/name — drop the separate
    // white titlebar strip (and the "E-Lab 700" repeated in it), keeping
    // only the red/orange/green traffic lights, which now float directly
    // over m_topBar. setLeftInset() keeps the logo/connection indicator from
    // sitting underneath that button cluster.
    applyMacTransparentTitlebar(this);
    m_topBar->setLeftInset(72);
#endif

    setCentralWidget(central);

    m_topBar->setGroupInfo(m_settings.currentClassName(), m_settings.currentGroupName());

    m_idleScreen = new IdleScreen(this);
    m_idleScreen->hide();

    // No hardcoded microscope model: the title comes from whatever the user
    // typed in the settings, live-updated.
    m_topBar->setMicroscopeName(m_settings.microscopeName());
    m_microscopeInfoPanel->setMicroscopeName(m_settings.microscopeName());
    connect(&m_settings, &AppSettings::microscopeNameChanged, m_topBar, &TopStatusBar::setMicroscopeName);
    connect(&m_settings, &AppSettings::microscopeNameChanged,
            m_microscopeInfoPanel, &MicroscopeInfoPanel::setMicroscopeName);

    // Runtime edition/feature flags: adjust the visible surface now and
    // whenever the user changes edition or toggles a feature in the panel.
    auto applyFeatureFlags = [this]() {
        m_topBar->setGroupButtonVisible(m_settings.featureClassesEnabled());
        m_topBar->setTeacherButtonToolTip(m_settings.appMode() == QLatin1String("school")
            ? tr("Mode professeur") : tr("Réglages avancés"));
        m_microscopeInfoPanel->setLearningAidsVisible(m_settings.featureLearningAidsEnabled());
    };
    applyFeatureFlags();
    connect(&m_settings, &AppSettings::featureFlagsChanged, this, applyFeatureFlags);

    m_idleTimer.setSingleShot(true);
    connect(&m_idleTimer, &QTimer::timeout, this, &MainWindow::showIdleScreen);
    connect(&m_settings, &AppSettings::idleTimeoutMinutesChanged, this, [this](int) { restartIdleTimer(); });
    connect(&m_settings, &AppSettings::lightThemeChanged, this, [](bool light) { applyThemeStylesheet(light); });
    connect(&m_settings, &AppSettings::lightThemeChanged, m_videoView, &VideoView::setLightTheme);
    m_videoView->setLightTheme(m_settings.lightTheme());

    connect(&m_settings, &AppSettings::showGridChanged, m_videoView, &VideoView::setShowGrid);
    m_videoView->setShowGrid(m_settings.showGrid());
    connect(&m_settings, &AppSettings::showFocusIndicatorChanged, m_videoView, &VideoView::setShowFocusIndicator);
    m_videoView->setShowFocusIndicator(m_settings.showFocusIndicator());
    connect(&m_settings, &AppSettings::showScaleBarChanged, m_videoView, &VideoView::setShowScaleBar);
    m_videoView->setShowScaleBar(m_settings.showScaleBar());
    connect(&m_settings, &AppSettings::scaleBarMicronsPer100PxChanged, m_videoView, &VideoView::setScaleBarCalibration);
    m_videoView->setScaleBarCalibration(m_settings.scaleBarMicronsPer100Px());

    connect(&m_timeLapseTimer, &QTimer::timeout, this, &MainWindow::onTimeLapseTick);
    connect(&m_labCountdownTimer, &QTimer::timeout, this, &MainWindow::onLabTimerTick);

    connect(&m_autoBackupCheckTimer, &QTimer::timeout, this, &MainWindow::checkAutoBackupDue);
    m_autoBackupCheckTimer.start(60 * 60 * 1000); // hourly is plenty for a check this cheap
    QTimer::singleShot(5000, this, &MainWindow::checkAutoBackupDue); // once shortly after launch too
    connect(&m_settings, &AppSettings::timeLapseEnabledChanged, this, [this](bool) { updateTimeLapseTimer(); });
    connect(&m_settings, &AppSettings::timeLapseIntervalSecondsChanged, this, [this](int) { updateTimeLapseTimer(); });
    updateTimeLapseTimer();

    qApp->installEventFilter(this);

    m_galleryModel.setFolder(m_settings.activeCaptureFolder());
    connect(&m_settings, &AppSettings::captureFolderChanged, this, [this](const QString &) {
        m_galleryModel.setFolder(m_settings.activeCaptureFolder());
    });
    connect(&m_settings, &AppSettings::activeCaptureFolderChanged, &m_galleryModel, &GalleryModel::setFolder);

    connect(&m_cameraManager, &CameraManager::connected, this, &MainWindow::onCameraConnected);
    connect(&m_cameraManager, &CameraManager::disconnected, this, &MainWindow::onCameraDisconnected);
    connect(&m_cameraManager, &CameraManager::poweredOnChanged, m_topBar, &TopStatusBar::setCameraPoweredIndicator);
    connect(&m_cameraManager, &CameraManager::poweredOnChanged, this, [this](bool on) {
        m_videoView->setCameraPoweredOff(!on);
    });
    m_topBar->setCameraPoweredIndicator(m_cameraManager.isPoweredOn());
    m_videoView->setCameraPoweredOff(!m_cameraManager.isPoweredOn());

    CameraBackend *backend = m_cameraManager.backend();
    connect(backend, &CameraBackend::frameReady, this, [this](const CameraFrame &frame) {
        CameraFrame processed = frame;
        processed.image = applyZoom(frame.image, m_zoomFactor);
        processed.image = applyMonochrome(processed.image, m_settings.monochromeDisplay());
        m_videoView->setFrame(processed.image);
        m_captureManager.pushFrame(processed);
        // Skipped entirely when the indicator is hidden — no reason to pay
        // for it every frame if nobody's looking at it. Smoothed (expo
        // moving average) rather than shown raw: frame-to-frame sensor
        // noise makes the raw score visibly jump/flicker every frame
        // otherwise, which reads as the whole display "jittering".
        if (m_settings.showFocusIndicator()) {
            const double rawScore = computeSharpnessScore(processed.image);
            m_smoothedFocusScore = 0.25 * rawScore + 0.75 * m_smoothedFocusScore;
            m_videoView->setFocusScore(m_smoothedFocusScore);
        }
    });
    connect(backend, &CameraBackend::fpsUpdated, m_topBar, &TopStatusBar::setFps);
    connect(backend, &CameraBackend::fpsUpdated, m_microscopeInfoPanel, &MicroscopeInfoPanel::setFps);
    connect(backend, &CameraBackend::fpsUpdated, this, [this](double fps) { m_lastReportedFps = fps; });
    connect(backend, &CameraBackend::errorOccurred, this, [this](const QString &message) {
        statusBar()->showMessage(message, 5000);
    });

    connect(m_bottomBar, &BottomBar::photoRequested, this, &MainWindow::onPhotoRequested);
    connect(m_bottomBar, &BottomBar::videoToggleRequested, this, &MainWindow::onVideoToggleRequested);
    connect(m_bottomBar, &BottomBar::autoRequested, this, &MainWindow::onAutoRequested);
    connect(m_bottomBar, &BottomBar::galleryRequested, this, &MainWindow::onGalleryRequested);
    connect(m_bottomBar, &BottomBar::fullscreenRequested, this, &MainWindow::onFullscreenRequested);
    connect(m_bottomBar, &BottomBar::analysisRequested, this, [this]() {
        AnalysisToolsDialog dialog(&m_settings, m_videoView->currentFrame(), this);
        dialog.exec();
    });

    // Metadata sidecar (.txt next to each photo, when enabled in the
    // settings): live camera state read here because CaptureManager itself
    // stays backend-agnostic.
    m_captureManager.setMetadataProvider([this]() {
        QString extra;
        const QString name = m_settings.microscopeName();
        extra += tr("Microscope : %1\n").arg(name.isEmpty() ? QStringLiteral("-") : name);
        CameraBackend *backend = m_cameraManager.backend();
        if (backend->isOpen()) {
            const QSize res = backend->currentResolution();
            extra += tr("Résolution capteur : %1 x %2\n").arg(res.width()).arg(res.height());
            extra += tr("Exposition : %1\n").arg(backend->exposure());
            extra += tr("Gain : %1\n").arg(backend->gain());
        }
        extra += tr("Étalonnage échelle : %1 µm / 100 px\n")
                     .arg(m_settings.scaleBarMicronsPer100Px(), 0, 'f', 1);
        return extra;
    });
    connect(m_bottomBar, &BottomBar::zoomInRequested, this, &MainWindow::onZoomInRequested);
    connect(m_bottomBar, &BottomBar::zoomOutRequested, this, &MainWindow::onZoomOutRequested);
    connect(m_bottomBar, &BottomBar::zoomResetRequested, this, &MainWindow::onZoomResetRequested);
    connect(m_topBar, &TopStatusBar::teacherModeRequested, this, &MainWindow::onTeacherModeRequested);
    connect(m_topBar, &TopStatusBar::microscopeInfoRequested, this, &MainWindow::onMicroscopeInfoRequested);
    connect(m_topBar, &TopStatusBar::resolutionClicked, this, &MainWindow::onResolutionClicked);
    connect(m_topBar, &TopStatusBar::connectionClicked, this, &MainWindow::onConnectionClicked);
    // Power toggle now lives inside the device-picker menu itself (see
    // onConnectionClicked) — the top-bar button merged with the connection
    // indicator, so there's no separate click target for it any more.
    // Goes through close() (not qApp->quit()) so closeEvent()'s PIN check
    // still applies when student-mode lock is on — a quit button shouldn't
    // be a bypass for the close button right next to it.
    connect(m_topBar, &TopStatusBar::quitRequested, this, &QWidget::close);
    connect(m_topBar, &TopStatusBar::helpRequested, this, [this]() {
        HelpDialog dialog(this);
        dialog.exec();
    });
    connect(m_microscopeInfoPanel, &MicroscopeInfoPanel::minimizeRequested, this, [this]() {
        setMicroscopeInfoPanelCollapsed(true);
    });
    connect(m_topBar, &TopStatusBar::groupSelectionRequested, this, &MainWindow::onGroupSelectionRequested);
    connect(m_videoView, &VideoView::doubleClicked, this, &MainWindow::onVideoDoubleClicked);
    connect(m_videoView, &VideoView::gridToggleClicked, this, [this]() { m_settings.setShowGrid(!m_settings.showGrid()); });

    connect(&m_captureManager, &CaptureManager::photoSaved, this, [this](const QString &path) {
        m_galleryModel.refresh();
        statusBar()->showMessage(tr("Photo enregistrée : %1").arg(path), 3000);
        // Time-lapse fires unattended on a timer — a rename prompt on every
        // shot would be far more disruptive than useful there.
        if (m_timeLapseCapturing)
            m_timeLapseCapturing = false;
        else
            promptRenamePhoto(path);
    });
    connect(&m_captureManager, &CaptureManager::recordingStopped, this, [this](const QString &path) {
        m_galleryModel.refresh();
        statusBar()->showMessage(tr("Vidéo enregistrée : %1").arg(path), 3000);
    });
    connect(&m_captureManager, &CaptureManager::captureError, this, [this](const QString &message) {
        QMessageBox::warning(this, QStringLiteral("E-Lab 700"), message);
    });

    onCameraDisconnected();
    restartIdleTimer();
}

void MainWindow::onPhotoRequested()
{
    m_captureManager.takePhoto();
}

void MainWindow::onVideoToggleRequested()
{
    if (m_captureManager.isRecording()) {
        m_captureManager.stopRecording();
        m_bottomBar->setRecording(false);
        if (m_settings.soundNotificationsEnabled())
            QApplication::beep();
    } else if (m_captureManager.startRecording()) {
        m_bottomBar->setRecording(true);
    }
}

void MainWindow::onAutoRequested()
{
    CameraBackend *backend = m_cameraManager.backend();
    backend->setAutoExposure(true);
    backend->setAutoWhiteBalance(true);
    statusBar()->showMessage(tr("Exposition et balance des blancs réglées automatiquement"), 2000);
    startResolutionAutoTuning();
}

void MainWindow::onZoomInRequested()
{
    m_zoomFactor = qMin(kMaxZoom, m_zoomFactor + kZoomStep);
    m_bottomBar->setZoomPercent(static_cast<int>(m_zoomFactor * 100));
}

void MainWindow::onZoomOutRequested()
{
    m_zoomFactor = qMax(kMinZoom, m_zoomFactor - kZoomStep);
    m_bottomBar->setZoomPercent(static_cast<int>(m_zoomFactor * 100));
}

void MainWindow::onZoomResetRequested()
{
    m_zoomFactor = kMinZoom;
    m_bottomBar->setZoomPercent(static_cast<int>(m_zoomFactor * 100));
}

void MainWindow::onGroupSelectionRequested()
{
    const QVector<SchoolClass> classes = m_settings.classes();
    if (classes.isEmpty()) {
        QMessageBox::information(this, tr("Se connecter"),
            tr("Aucune classe n'est configurée. Demande à ton enseignant d'ouvrir le mode "
               "professeur et \"Gérer les classes et groupes...\"."));
        return;
    }

    StartupSelectionDialog dialog(classes, this);
    if (dialog.exec() == QDialog::Accepted) {
        m_settings.setActiveGroup(dialog.selectedClassName(), dialog.selectedGroupName());
        m_topBar->setGroupInfo(m_settings.currentClassName(), m_settings.currentGroupName());
    }
}

void MainWindow::onGalleryRequested()
{
    GalleryDialog dialog(&m_galleryModel, &m_settings, this);
    dialog.exec();
}

void MainWindow::startResolutionAutoTuning()
{
    CameraBackend *backend = m_cameraManager.backend();
    if (!backend->isOpen())
        return;

    QVector<QSize> candidates = backend->supportedResolutions();
    // Highest pixel count first: we want the sharpest resolution that still
    // sustains a usable live frame rate, so start optimistic and step down.
    std::sort(candidates.begin(), candidates.end(), [](const QSize &a, const QSize &b) {
        return a.width() * a.height() > b.width() * b.height();
    });
    candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());
    m_resolutionProbeCandidates = candidates;
    m_resolutionProbeIndex = 0;
    probeNextResolution();
}

void MainWindow::probeNextResolution()
{
    if (m_resolutionProbeIndex >= m_resolutionProbeCandidates.size())
        return;

    CameraBackend *backend = m_cameraManager.backend();
    if (!backend->isOpen())
        return;

    const QSize candidate = m_resolutionProbeCandidates.at(m_resolutionProbeIndex);
    backend->setResolution(candidate);
    m_lastReportedFps = 0.0;

    // Long enough for at least one real fpsUpdated tick from the backend
    // (which measures over ~1s windows) to land before judging this
    // candidate.
    QTimer::singleShot(1300, this, [this]() {
        CameraBackend *liveBackend = m_cameraManager.backend();
        if (!liveBackend->isOpen())
            return;

        constexpr double kMinAcceptableFps = 6.0;
        const bool lastCandidate = (m_resolutionProbeIndex == m_resolutionProbeCandidates.size() - 1);
        if (m_lastReportedFps >= kMinAcceptableFps || lastCandidate) {
            const QSize chosen = liveBackend->currentResolution();
            m_topBar->setResolution(chosen);
            m_microscopeInfoPanel->setResolution(chosen);
            statusBar()->showMessage(
                tr("Résolution optimisée automatiquement : %1 x %2 (%3 ips)")
                    .arg(chosen.width()).arg(chosen.height()).arg(m_lastReportedFps, 0, 'f', 1),
                4000);
            return;
        }
        ++m_resolutionProbeIndex;
        probeNextResolution();
    });
}

void MainWindow::onFullscreenRequested()
{
    if (isFullScreen()) {
        if (m_immersiveMode)
            setImmersiveMode(false);
        showNormal();
    } else {
        showFullScreen();
    }
}

void MainWindow::onVideoDoubleClicked()
{
    if (!isFullScreen())
        showFullScreen();
    setImmersiveMode(!m_immersiveMode);
}

void MainWindow::setImmersiveMode(bool immersive)
{
    m_immersiveMode = immersive;
    m_topBar->setVisible(!immersive);
    m_bottomBar->setVisible(!immersive);
    m_videoView->setImmersive(immersive);
    // Immersive/fullscreen is meant to be just the video, full stop — no
    // side panel eating into it either way. Leaving immersive restores
    // whichever expanded/collapsed state it was actually in beforehand
    // (remembered separately so this doesn't clobber the user's own
    // collapse/expand choice). Instant either way (animate=false): this is
    // a mode switch, not a user-initiated collapse/expand.
    if (immersive) {
        m_microscopeInfoCollapsedBeforeImmersive = m_microscopeInfoCollapsed;
        setMicroscopeInfoPanelCollapsed(true, false);
    } else {
        setMicroscopeInfoPanelCollapsed(m_microscopeInfoCollapsedBeforeImmersive, false);
    }
}

bool MainWindow::confirmTeacherPin()
{
    bool ok = false;
    const QString pin = QInputDialog::getText(this, tr("Mode professeur"),
                                               tr("Code PIN professeur :"),
                                               QLineEdit::Password, QString(), &ok);
    if (!ok)
        return false;

    if (!m_settings.checkTeacherPin(pin)) {
        QMessageBox::warning(this, tr("Mode professeur"), tr("Code PIN incorrect."));
        return false;
    }
    return true;
}

void MainWindow::onTeacherModeRequested()
{
    // Only actually gate on a PIN that was deliberately created (the "Changer"
    // form in Fonctionnalités/Sécurité) — never on a hidden pre-seeded
    // default, so ticking "Code PIN professeur" alone can't lock anyone out
    // of a PIN nobody chose yet.
    if (m_settings.featurePinLockEnabled() && !m_settings.teacherPinHash().isEmpty() && !confirmTeacherPin())
        return;

    TeacherPanel panel(&m_settings, m_cameraManager.backend(),
                        [this]() { return m_videoView->currentFrame(); }, this);
    connect(&panel, &TeacherPanel::labTimerStartRequested, this, &MainWindow::startLabTimer);
    connect(&panel, &TeacherPanel::labTimerStopRequested, this, &MainWindow::stopLabTimer);
    panel.exec();

    // Resolution or capture folder may have changed while the panel was open.
    m_topBar->setResolution(m_cameraManager.backend()->currentResolution());
    m_microscopeInfoPanel->setResolution(m_cameraManager.backend()->currentResolution());
}

void MainWindow::onCameraConnected(const CameraDeviceInfo &info)
{
    CameraBackend *backend = m_cameraManager.backend();
    m_topBar->setConnected(true, info.model);
    m_topBar->setResolution(backend->currentResolution());
    m_videoView->setCameraConnected(true);
    m_microscopeInfoPanel->setConnected(true);
    m_microscopeInfoPanel->setResolution(backend->currentResolution());
    if (m_userChoseResolution) {
        // Respect the teacher/student's own pick across reconnects instead
        // of silently overriding it with the auto-tuner below. Deferred
        // (not called synchronously here): reconfiguring the capture format
        // in the same call stack as open() — right as the DirectShow graph
        // is still settling — was unstable on some drivers (crashed on
        // camera select). A short pause first lets it stabilize.
        QTimer::singleShot(400, this, [this]() {
            CameraBackend *liveBackend = m_cameraManager.backend();
            if (!liveBackend->isOpen())
                return;
            liveBackend->setResolution(m_userChosenResolution);
            const QSize applied = liveBackend->currentResolution();
            m_topBar->setResolution(applied);
            m_microscopeInfoPanel->setResolution(applied);
        });
    } else if (!m_autoTuningDoneOnce) {
        // Sharp in the eyepieces but soft on screen usually means the
        // captured resolution is lower than what's needed to fill the
        // display without upscaling blur — pick the highest resolution the
        // camera can sustain at a usable live frame rate automatically,
        // rather than leaving everyone stuck with whatever conservative
        // default was requested at open() time. Only ever done once per
        // session (see m_autoTuningDoneOnce) — cycling setResolution()
        // through several candidates is exactly the kind of repeated format
        // renegotiation that proved unstable, so later reconnects/device
        // switches just keep whatever resolution they open with instead.
        m_autoTuningDoneOnce = true;
        QTimer::singleShot(400, this, [this]() { startResolutionAutoTuning(); });
    }
}

void MainWindow::onCameraDisconnected()
{
    m_topBar->setConnected(false);
    m_videoView->setCameraConnected(false);
    m_microscopeInfoPanel->setConnected(false);
    m_microscopeInfoPanel->setResolution(QSize());
}

void MainWindow::onMicroscopeInfoRequested()
{
    setMicroscopeInfoPanelCollapsed(!m_microscopeInfoCollapsed);
}

namespace {
// Small filled-circle icon so a picker menu entry shows its state at a
// glance (green = this is the current one, red = an alternative) instead of
// just a checkmark.
QIcon coloredDotIcon(const QColor &color)
{
    QPixmap pixmap(16, 16);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    painter.drawEllipse(2, 2, 12, 12);
    return QIcon(pixmap);
}

// The microscope's native presets (640x480, 1280x960, 2592x1944) are all
// 4:3; a laptop's own built-in webcam is typically 16:9 (e.g. 1280x720) or
// some other non-4:3 shape. Used only to decide whether picking a device
// from the connection menu should require the teacher PIN (see
// onConnectionClicked) — a heuristic, not a hard filter, since the device
// list itself still shows whatever was actually detected.
bool looksLikeMicroscopeAspectRatio(const QSize &resolution)
{
    if (resolution.height() <= 0)
        return false;
    const double ratio = static_cast<double>(resolution.width()) / resolution.height();
    return qAbs(ratio - 4.0 / 3.0) < 0.05;
}
}

void MainWindow::onResolutionClicked()
{
    CameraBackend *backend = m_cameraManager.backend();
    if (!backend->isOpen()) {
        statusBar()->showMessage(tr("Aucune caméra connectée."), 3000);
        return;
    }

    const QVector<QSize> resolutions = backend->supportedResolutions();
    if (resolutions.isEmpty())
        return;

    const QSize current = backend->currentResolution();
    QMenu menu(this);
    for (const QSize &size : resolutions) {
        const bool isCurrent = (size == current);
        QAction *action = menu.addAction(coloredDotIcon(isCurrent ? QColor("#35e08a") : QColor("#ff5c6c")),
                                          QStringLiteral("%1 x %2").arg(size.width()).arg(size.height()));
        connect(action, &QAction::triggered, this, [this, size]() {
            // A manual pick sticks — startResolutionAutoTuning() (run on
            // every fresh connect) no longer overrides it on a later
            // reconnect within this session (see onCameraConnected()).
            m_userChoseResolution = true;
            m_userChosenResolution = size;

            CameraBackend *liveBackend = m_cameraManager.backend();
            if (!liveBackend->setResolution(size))
                return;
            const QSize applied = liveBackend->currentResolution();
            m_topBar->setResolution(applied);
            m_microscopeInfoPanel->setResolution(applied);
            if (applied == size) {
                statusBar()->showMessage(
                    tr("Résolution : %1 x %2").arg(applied.width()).arg(applied.height()), 3000);
            } else {
                // Some drivers silently ignore an unsupported resolution
                // instead of failing setResolution() outright — say so
                // rather than claim success while nothing actually changed.
                statusBar()->showMessage(
                    tr("La caméra n'a pas accepté cette résolution (reste à %1 x %2).")
                        .arg(applied.width()).arg(applied.height()),
                    5000);
            }
        });
    }
    menu.exec(QCursor::pos());
}

void MainWindow::onConnectionClicked()
{
    // Prefer devices that look like the microscope (see CameraManager::
    // microscopeDevices) so the laptop's own webcam isn't offered as a
    // choice — but if that filter leaves nothing (e.g. the microscope
    // briefly negotiated a lower resolution during the probe than its true
    // native one) fall back to every detected device rather than block
    // manual selection entirely, which used to be the only way to recover
    // when auto-detection guessed wrong.
    QVector<CameraDeviceInfo> devices = m_cameraManager.microscopeDevices();
    const bool showingAllDevices = devices.isEmpty() && !m_cameraManager.lastKnownDevices().isEmpty();
    if (showingAllDevices)
        devices = m_cameraManager.lastKnownDevices();

    QMenu menu(this);

    // Right at the top, next to the camera's own name/status — click to
    // turn it on or off. This is the one control that actually stops the
    // camera being touched at all (see setPoweredOn/rescan), not just a
    // "disconnect" that leaves it idling: turning it off is meant to spare
    // the camera from being probed/opened needlessly when nobody's using it.
    const bool poweredOn = m_cameraManager.isPoweredOn();
    QAction *powerAction = menu.addAction(coloredDotIcon(poweredOn ? QColor("#35e08a") : QColor("#ff5c6c")),
                                           poweredOn ? tr("Caméra allumée (cliquer pour éteindre)")
                                                     : tr("Caméra éteinte (cliquer pour allumer)"));
    connect(powerAction, &QAction::triggered, this, [this]() {
        m_cameraManager.setPoweredOn(!m_cameraManager.isPoweredOn());
    });
    menu.addSeparator();

    if (m_cameraManager.isConnected()) {
        QAction *disconnectAction = menu.addAction(tr("Déconnecter la caméra"));
        connect(disconnectAction, &QAction::triggered, this, [this]() {
            m_cameraManager.disconnectCamera();
            statusBar()->showMessage(tr("Caméra déconnectée."), 3000);
        });
        menu.addSeparator();
    }

    if (devices.isEmpty()) {
        QAction *none = menu.addAction(tr("Aucune caméra détectée"));
        none->setEnabled(false);
    } else {
        if (showingAllDevices) {
            QAction *note = menu.addAction(tr("Aucune caméra ne ressemble au microscope — tout est affiché"));
            note->setEnabled(false);
        }
        // "Nom du microscope" (Réglages) replaces the generic "Caméra USB"
        // label when set, e.g. "OMAX Caméra" — same setting already used
        // for the top-bar title, reused here instead of a second field.
        const QString microscopeName = m_settings.microscopeName().trimmed();
        for (const CameraDeviceInfo &device : devices) {
            const bool isCurrent = m_cameraManager.isConnected() && device.id == m_cameraManager.currentDeviceId();
            const QString label = microscopeName.isEmpty()
                ? device.displayName
                : tr("%1 (%2x%3)").arg(microscopeName).arg(device.resolution.width()).arg(device.resolution.height());
            QAction *action = menu.addAction(coloredDotIcon(isCurrent ? QColor("#35e08a") : QColor("#ff5c6c")),
                                              label);
            action->setCheckable(true);
            action->setChecked(isCurrent);
            const bool needsPin = !looksLikeMicroscopeAspectRatio(device.resolution)
                && m_settings.featurePinLockEnabled() && !m_settings.teacherPinHash().isEmpty();
            connect(action, &QAction::triggered, this, [this, device, needsPin]() {
                // Devices that don't have the microscope's 4:3 shape are most
                // likely a laptop's own built-in webcam — gate those behind
                // the teacher PIN so a student can't switch away from the
                // microscope to it (accidentally or otherwise) from this menu.
                if (needsPin && !confirmTeacherPin())
                    return;
                if (!m_cameraManager.forceConnect(device.id)) {
                    statusBar()->showMessage(
                        tr("Impossible de se connecter à %1.").arg(device.displayName), 4000);
                }
            });
        }
    }

    menu.exec(QCursor::pos());
}

void MainWindow::setMicroscopeInfoPanelCollapsed(bool collapsed, bool animate)
{
    if (m_microscopeInfoCollapsed == collapsed && !m_microscopeInfoAnim)
        return;
    m_microscopeInfoCollapsed = collapsed;

    const int targetWidth = collapsed ? 0 : m_microscopeInfoExpandedWidth;

    // Any in-flight toggle loses — a fresh one always wins over whatever
    // was still animating, instead of the two fighting over the same
    // properties frame by frame.
    if (m_microscopeInfoAnim) {
        m_microscopeInfoAnim->stop();
        m_microscopeInfoAnim->deleteLater();
        m_microscopeInfoAnim = nullptr;
    }

    if (!animate) {
        m_microscopeInfoContainer->setMinimumWidth(targetWidth);
        m_microscopeInfoContainer->setMaximumWidth(targetWidth);
        m_microscopeInfoContainer->setVisible(!collapsed);
        return;
    }

    // Needs to stay visible for the shrink to actually be seen — only
    // hidden once the group finishes (see below), and only when collapsing.
    if (!collapsed)
        m_microscopeInfoContainer->setVisible(true);

    auto *group = new QParallelAnimationGroup(this);
    constexpr int kDurationMs = 320;

    auto *minAnim = new QPropertyAnimation(m_microscopeInfoContainer, "minimumWidth", group);
    minAnim->setDuration(kDurationMs);
    minAnim->setStartValue(m_microscopeInfoContainer->minimumWidth());
    minAnim->setEndValue(targetWidth);
    minAnim->setEasingCurve(QEasingCurve::OutCubic);
    group->addAnimation(minAnim);

    auto *maxAnim = new QPropertyAnimation(m_microscopeInfoContainer, "maximumWidth", group);
    maxAnim->setDuration(kDurationMs);
    maxAnim->setStartValue(m_microscopeInfoContainer->maximumWidth());
    maxAnim->setEndValue(targetWidth);
    maxAnim->setEasingCurve(QEasingCurve::OutCubic);
    group->addAnimation(maxAnim);

    if (collapsed) {
        connect(group, &QParallelAnimationGroup::finished, this, [this]() {
            m_microscopeInfoContainer->setVisible(false);
        });
    }
    connect(group, &QParallelAnimationGroup::finished, this, [this, group]() {
        if (m_microscopeInfoAnim == group)
            m_microscopeInfoAnim = nullptr;
    });

    m_microscopeInfoAnim = group;
    group->start(QAbstractAnimation::DeleteWhenStopped);
}

void MainWindow::promptRenamePhoto(const QString &path)
{
    const QFileInfo info(path);
    bool ok = false;
    const QString newBaseName = QInputDialog::getText(this, tr("Renommer la photo"),
        tr("Nom du fichier :"), QLineEdit::Normal, info.completeBaseName(), &ok);
    if (!ok)
        return;

    const QString sanitized = sanitizeForFileName(newBaseName);
    if (sanitized.isEmpty() || sanitized == info.completeBaseName())
        return;

    const QString newPath = info.dir().filePath(sanitized + QLatin1Char('.') + info.suffix());
    if (QFile::exists(newPath)) {
        QMessageBox::warning(this, tr("Renommer"), tr("Un fichier porte déjà ce nom."));
        return;
    }

    if (QFile::rename(path, newPath))
        m_galleryModel.refresh();
    else
        QMessageBox::warning(this, tr("Renommer"), tr("Impossible de renommer le fichier."));
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_settings.studentModeLocked() && !m_settings.teacherPinHash().isEmpty() && !confirmTeacherPin()) {
        event->ignore();
        return;
    }
    event->accept();
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    m_idleScreen->setGeometry(rect());
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape && m_immersiveMode) {
        setImmersiveMode(false);
        event->accept();
        return;
    }
    QMainWindow::keyPressEvent(event);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    switch (event->type()) {
    case QEvent::MouseMove:
    case QEvent::MouseButtonPress:
    case QEvent::KeyPress:
        restartIdleTimer();
        break;
    default:
        break;
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::restartIdleTimer()
{
    if (m_idleScreen->isVisible())
        m_idleScreen->hide();

    m_idleTimer.stop();
    const int minutes = m_settings.idleTimeoutMinutes();
    if (minutes > 0)
        m_idleTimer.start(minutes * 60000);
}

void MainWindow::updateTimeLapseTimer()
{
    if (m_settings.timeLapseEnabled())
        m_timeLapseTimer.start(m_settings.timeLapseIntervalSeconds() * 1000);
    else
        m_timeLapseTimer.stop();
}

void MainWindow::onTimeLapseTick()
{
    // Silently skip rather than surfacing an error dialog while unattended —
    // captureError's QMessageBox would otherwise pop up on its own every
    // interval if the camera happens to be disconnected.
    if (!m_cameraManager.backend()->isOpen())
        return;

    m_timeLapseCapturing = true;
    m_captureManager.takePhoto();
    // Reset unconditionally: if takePhoto() failed (e.g. no frame yet) it
    // emits captureError instead of photoSaved, so the flag would otherwise
    // never get cleared and could wrongly suppress the next manual photo's
    // rename prompt.
    m_timeLapseCapturing = false;
}

void MainWindow::startLabTimer(int minutes)
{
    m_labSecondsRemaining = minutes * 60;
    updateLabTimerDisplay();
    m_labCountdownTimer.start(1000);
}

void MainWindow::stopLabTimer()
{
    m_labCountdownTimer.stop();
    m_labSecondsRemaining = 0;
    m_videoView->setLabTimerText(QString());
}

void MainWindow::checkAutoBackupDue()
{
    if (!m_settings.autoBackupEnabled())
        return;

    const QString destinationRoot = m_settings.autoBackupDestination();
    if (destinationRoot.isEmpty())
        return;

    const QDateTime last = m_settings.lastAutoBackupAt();
    const qint64 daysSinceLast = last.isValid() ? last.daysTo(QDateTime::currentDateTime())
                                                 : std::numeric_limits<qint64>::max();
    if (daysSinceLast < m_settings.autoBackupIntervalDays())
        return;

    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
    const QString destination = QDir(destinationRoot).filePath(tr("E-Lab700_Sauvegarde_%1").arg(timestamp));

    if (FileUtils::copyFolderRecursively(m_settings.captureFolder(), destination, GalleryModel::trashFolderName())) {
        m_settings.setLastAutoBackupAt(QDateTime::currentDateTime());
        statusBar()->showMessage(tr("Sauvegarde automatique effectuée."), 4000);
    }
    // Silent on failure (e.g. destination drive unplugged) — this runs
    // unattended, and a background failure shouldn't interrupt class with a
    // dialog; the teacher can check "Dernière sauvegarde" in their panel if
    // captures seem to not be backing up.
}

void MainWindow::onLabTimerTick()
{
    if (m_labSecondsRemaining <= 0) {
        m_labCountdownTimer.stop();
        statusBar()->showMessage(tr("Minuteur terminé !"), 5000);
        if (m_settings.soundNotificationsEnabled())
            QApplication::beep();
        return;
    }
    --m_labSecondsRemaining;
    updateLabTimerDisplay();
}

void MainWindow::updateLabTimerDisplay()
{
    const int minutes = m_labSecondsRemaining / 60;
    const int seconds = m_labSecondsRemaining % 60;
    m_videoView->setLabTimerText(
        QStringLiteral("%1:%2").arg(minutes, 2, 10, QLatin1Char('0')).arg(seconds, 2, 10, QLatin1Char('0')));
}

void MainWindow::showIdleScreen()
{
    m_idleScreen->setGeometry(rect());
    m_idleScreen->raise();
    m_idleScreen->show();
}
