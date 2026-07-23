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
#include <QFile>
#include <QFileInfo>
#include <QIcon>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPixmap>
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
    return qBound(0.0, std::sqrt(qMax(0.0, variance)) * 2.0, 100.0);
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
    QTextStream stream(&file);
    qApp->setStyleSheet(stream.readAll());
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

    layout->addWidget(m_topBar);
    layout->addWidget(m_videoView, 1);
    layout->addWidget(m_bottomBar);

    setCentralWidget(central);

    m_topBar->setGroupInfo(m_settings.currentClassName(), m_settings.currentGroupName());

    m_idleScreen = new IdleScreen(this);
    m_idleScreen->hide();

    m_microscopeInfoPanel = new MicroscopeInfoPanel(central);
    m_microscopeInfoPanel->hide();

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
            ? QStringLiteral("Mode professeur") : QStringLiteral("Réglages avancés"));
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
        extra += QStringLiteral("Microscope : %1\n").arg(name.isEmpty() ? QStringLiteral("-") : name);
        CameraBackend *backend = m_cameraManager.backend();
        if (backend->isOpen()) {
            const QSize res = backend->currentResolution();
            extra += QStringLiteral("Résolution capteur : %1 x %2\n").arg(res.width()).arg(res.height());
            extra += QStringLiteral("Exposition : %1\n").arg(backend->exposure());
            extra += QStringLiteral("Gain : %1\n").arg(backend->gain());
        }
        extra += QStringLiteral("Étalonnage échelle : %1 µm / 100 px\n")
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
    connect(m_topBar, &TopStatusBar::helpRequested, this, [this]() {
        HelpDialog dialog(this);
        dialog.exec();
    });
    connect(m_microscopeInfoPanel, &MicroscopeInfoPanel::closeRequested, m_microscopeInfoPanel, &QWidget::hide);
    connect(m_topBar, &TopStatusBar::groupSelectionRequested, this, &MainWindow::onGroupSelectionRequested);
    connect(m_videoView, &VideoView::doubleClicked, this, &MainWindow::onVideoDoubleClicked);
    connect(m_videoView, &VideoView::gridToggleClicked, this, [this]() { m_settings.setShowGrid(!m_settings.showGrid()); });

    connect(&m_captureManager, &CaptureManager::photoSaved, this, [this](const QString &path) {
        m_galleryModel.refresh();
        statusBar()->showMessage(QStringLiteral("Photo enregistrée : %1").arg(path), 3000);
        // Time-lapse fires unattended on a timer — a rename prompt on every
        // shot would be far more disruptive than useful there.
        if (m_timeLapseCapturing)
            m_timeLapseCapturing = false;
        else
            promptRenamePhoto(path);
    });
    connect(&m_captureManager, &CaptureManager::recordingStopped, this, [this](const QString &path) {
        m_galleryModel.refresh();
        statusBar()->showMessage(QStringLiteral("Vidéo enregistrée : %1").arg(path), 3000);
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
    } else if (m_captureManager.startRecording()) {
        m_bottomBar->setRecording(true);
    }
}

void MainWindow::onAutoRequested()
{
    CameraBackend *backend = m_cameraManager.backend();
    backend->setAutoExposure(true);
    backend->setAutoWhiteBalance(true);
    statusBar()->showMessage(QStringLiteral("Exposition et balance des blancs réglées automatiquement"), 2000);
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
        QMessageBox::information(this, QStringLiteral("Se connecter"),
            QStringLiteral("Aucune classe n'est configurée. Demande à ton enseignant d'ouvrir le mode "
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
                QStringLiteral("Résolution optimisée automatiquement : %1 x %2 (%3 ips)")
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
    if (immersive && m_microscopeInfoPanel->isVisible())
        m_microscopeInfoPanel->hide();
}

bool MainWindow::confirmTeacherPin()
{
    bool ok = false;
    const QString pin = QInputDialog::getText(this, QStringLiteral("Mode professeur"),
                                               QStringLiteral("Code PIN professeur :"),
                                               QLineEdit::Password, QString(), &ok);
    if (!ok)
        return false;

    if (!m_settings.checkTeacherPin(pin)) {
        QMessageBox::warning(this, QStringLiteral("Mode professeur"), QStringLiteral("Code PIN incorrect."));
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
    // Sharp in the eyepieces but soft on screen usually means the captured
    // resolution is lower than what's needed to fill the display without
    // upscaling blur — pick the highest resolution the camera can sustain
    // at a usable live frame rate automatically, rather than leaving
    // everyone stuck with whatever conservative default was requested at
    // open() time.
    startResolutionAutoTuning();
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
    if (m_microscopeInfoPanel->isVisible()) {
        m_microscopeInfoPanel->hide();
        return;
    }
    positionMicroscopeInfoPanel();
    m_microscopeInfoPanel->raise();
    m_microscopeInfoPanel->show();
}

void MainWindow::onResolutionClicked()
{
    CameraBackend *backend = m_cameraManager.backend();
    if (!backend->isOpen())
        return;

    const QVector<QSize> resolutions = backend->supportedResolutions();
    if (resolutions.isEmpty())
        return;

    const QSize current = backend->currentResolution();
    QMenu menu(this);
    for (const QSize &size : resolutions) {
        QAction *action = menu.addAction(QStringLiteral("%1 x %2").arg(size.width()).arg(size.height()));
        action->setCheckable(true);
        action->setChecked(size == current);
        connect(action, &QAction::triggered, this, [this, size]() {
            CameraBackend *liveBackend = m_cameraManager.backend();
            if (!liveBackend->setResolution(size))
                return;
            const QSize applied = liveBackend->currentResolution();
            m_topBar->setResolution(applied);
            m_microscopeInfoPanel->setResolution(applied);
            statusBar()->showMessage(
                QStringLiteral("Résolution : %1 x %2").arg(applied.width()).arg(applied.height()), 3000);
        });
    }
    menu.exec(QCursor::pos());
}

namespace {
// Small filled-circle icon so each device in the picker menu shows its live
// connection state at a glance (green = this is the one currently open, red
// = detected but not connected) instead of just a checkmark.
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
}

void MainWindow::onConnectionClicked()
{
    const QVector<CameraDeviceInfo> devices = m_cameraManager.lastKnownDevices();
    QMenu menu(this);

    if (m_cameraManager.isConnected()) {
        QAction *disconnectAction = menu.addAction(QStringLiteral("Déconnecter la caméra"));
        connect(disconnectAction, &QAction::triggered, this, [this]() {
            m_cameraManager.disconnectCamera();
            statusBar()->showMessage(QStringLiteral("Caméra déconnectée."), 3000);
        });
        menu.addSeparator();
    }

    if (devices.isEmpty()) {
        QAction *none = menu.addAction(QStringLiteral("Aucune caméra détectée"));
        none->setEnabled(false);
    } else {
        for (const CameraDeviceInfo &device : devices) {
            const bool isCurrent = m_cameraManager.isConnected() && device.id == m_cameraManager.currentDeviceId();
            QAction *action = menu.addAction(coloredDotIcon(isCurrent ? QColor("#35e08a") : QColor("#ff5c6c")),
                                              device.displayName);
            action->setCheckable(true);
            action->setChecked(isCurrent);
            connect(action, &QAction::triggered, this, [this, device]() {
                if (!m_cameraManager.forceConnect(device.id)) {
                    statusBar()->showMessage(
                        QStringLiteral("Impossible de se connecter à %1.").arg(device.displayName), 4000);
                }
            });
        }
    }

    menu.exec(QCursor::pos());
}

void MainWindow::positionMicroscopeInfoPanel()
{
    // The panel is a plain child widget, not managed by any layout of its
    // own parent, so nothing ever resizes it to fit its content the way a
    // layout-managed widget would — without this it kept whatever tiny
    // default size it had at construction time, rendering as a near-empty
    // sliver instead of the actual card.
    m_microscopeInfoPanel->adjustSize();

    // Positioned as a small card next to the video image, near its top-right
    // corner, rather than a modal dialog — so students can keep watching the
    // live feed while reading the specs.
    const QRect videoGeometry = m_videoView->geometry();
    const int x = videoGeometry.right() - m_microscopeInfoPanel->width() - 24;
    const int y = videoGeometry.top() + 24;
    m_microscopeInfoPanel->move(std::max(videoGeometry.left() + 12, x), y);
}

void MainWindow::promptRenamePhoto(const QString &path)
{
    const QFileInfo info(path);
    bool ok = false;
    const QString newBaseName = QInputDialog::getText(this, QStringLiteral("Renommer la photo"),
        QStringLiteral("Nom du fichier :"), QLineEdit::Normal, info.completeBaseName(), &ok);
    if (!ok)
        return;

    const QString sanitized = sanitizeForFileName(newBaseName);
    if (sanitized.isEmpty() || sanitized == info.completeBaseName())
        return;

    const QString newPath = info.dir().filePath(sanitized + QLatin1Char('.') + info.suffix());
    if (QFile::exists(newPath)) {
        QMessageBox::warning(this, QStringLiteral("Renommer"), QStringLiteral("Un fichier porte déjà ce nom."));
        return;
    }

    if (QFile::rename(path, newPath))
        m_galleryModel.refresh();
    else
        QMessageBox::warning(this, QStringLiteral("Renommer"), QStringLiteral("Impossible de renommer le fichier."));
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
    if (m_microscopeInfoPanel->isVisible())
        positionMicroscopeInfoPanel();
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
    const QString destination = QDir(destinationRoot).filePath(QStringLiteral("E-Lab700_Sauvegarde_%1").arg(timestamp));

    if (FileUtils::copyFolderRecursively(m_settings.captureFolder(), destination, GalleryModel::trashFolderName())) {
        m_settings.setLastAutoBackupAt(QDateTime::currentDateTime());
        statusBar()->showMessage(QStringLiteral("Sauvegarde automatique effectuée."), 4000);
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
        statusBar()->showMessage(QStringLiteral("Minuteur terminé !"), 5000);
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
