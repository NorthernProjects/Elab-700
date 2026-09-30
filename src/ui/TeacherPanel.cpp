#include "TeacherPanel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFontMetrics>
#include <QFormLayout>
#include <QGroupBox>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScreen>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>

#include "ClassManagerDialog.h"
#include "ScaleCalibrationDialog.h"
#include "SmoothScrollArea.h"
#include "TeacherOverviewDialog.h"
#include "core/FileUtils.h"
#include "core/GalleryModel.h"

namespace {
// Sizing spin boxes/labels off a fixed pixel guess kept clipping the digits
// on some displays (font/DPI rendering this widget never directly
// accounts for) — measuring the actual widest string this widget will ever
// show, in its own real font, and adding generous room for the up/down
// spin arrows is what actually stays correct regardless of the display.
int widthForWidestText(const QWidget *widget, const QString &widestText, int extraPadding)
{
    const QFontMetrics metrics(widget->font());
    return metrics.horizontalAdvance(widestText) + extraPadding;
}
}

TeacherPanel::TeacherPanel(AppSettings *settings, CameraBackend *backend, std::function<QImage()> frameProvider,
                           QWidget *parent)
    : QDialog(parent), m_settings(settings), m_backend(backend), m_frameProvider(std::move(frameProvider))
{
    setWindowTitle(m_settings->appMode() == QLatin1String("school")
        ? tr("Mode professeur") : tr("Réglages avancés"));

    // Take up most of the screen instead of a small fixed size: with two
    // columns of settings groups below, this puts nearly everything on
    // screen at once instead of a long single-column scroll.
    if (QScreen *screen = QGuiApplication::primaryScreen()) {
        const QRect avail = screen->availableGeometry();
        const int w = qBound(900, static_cast<int>(avail.width() * 0.85), 1500);
        const int h = qBound(700, static_cast<int>(avail.height() * 0.88), 1000);
        resize(w, h);
    } else {
        resize(1100, 800);
    }

    auto *root = new QVBoxLayout(this);

    auto *scrollArea = new SmoothScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    root->addWidget(scrollArea, 1);

    auto *scrollContent = new QWidget(scrollArea);
    scrollArea->setWidget(scrollContent);
    auto *content = new QVBoxLayout(scrollContent);

    m_cameraInfoLabel = new QLabel(scrollContent);
    m_cameraInfoLabel->setWordWrap(true);
    content->addWidget(m_cameraInfoLabel);

    // Two columns side by side (instead of one long stack) so far less
    // scrolling is needed to see everything.
    auto *columns = new QHBoxLayout();
    columns->setSpacing(20);
    auto *leftColumn = new QVBoxLayout();
    auto *rightColumn = new QVBoxLayout();
    columns->addLayout(leftColumn, 1);
    columns->addLayout(rightColumn, 1);
    content->addLayout(columns);

    // ---- Édition du logiciel (un seul exe, trois profils gratuits) ----
    auto *editionGroup = new QGroupBox(tr("Édition du logiciel"), scrollContent);
    auto *editionLayout = new QVBoxLayout(editionGroup);
    auto *editionIntro = new QLabel(
        tr("La version de base est adaptée au grand public, au scolaire et au laboratoire — "
           "chaque édition active les options qui lui conviennent."),
        editionGroup);
    editionIntro->setWordWrap(true);
    editionLayout->addWidget(editionIntro);

    auto *editionCombo = new QComboBox(editionGroup);
    editionCombo->addItem(tr("Scolaire (salle de classe)"), QStringLiteral("school"));
    editionCombo->addItem(tr("Grand public"), QStringLiteral("public"));
    editionCombo->addItem(tr("Laboratoire"), QStringLiteral("lab"));
    const int editionIndex = editionCombo->findData(m_settings->appMode());
    editionCombo->setCurrentIndex(editionIndex >= 0 ? editionIndex : 1);
    editionLayout->addWidget(editionCombo);

    // ---- Fonctionnalités supplémentaires (emprunter aux autres éditions) ----
    auto *featuresGroup = new QGroupBox(tr("Fonctionnalités supplémentaires"), scrollContent);
    auto *featuresLayout = new QVBoxLayout(featuresGroup);
    auto *featuresIntro = new QLabel(
        tr("Ajoutez à votre édition les fonctionnalités des autres (changer d'édition ci-dessus "
           "remet ces cases aux valeurs par défaut de l'édition choisie) :"),
        featuresGroup);
    featuresIntro->setWordWrap(true);
    featuresLayout->addWidget(featuresIntro);

    auto *featureClassesCheck = new QCheckBox(tr("Classes et groupes (scolaire)"), featuresGroup);
    auto *featurePinCheck = new QCheckBox(tr("Code PIN professeur et verrouillage (scolaire)"), featuresGroup);
    auto *featureTimerCheck = new QCheckBox(tr("Minuteur d'observation (scolaire)"), featuresGroup);
    auto *featureAidsCheck = new QCheckBox(tr("Schéma du microscope et glossaire (scolaire/grand public)"), featuresGroup);
    auto *featureLabToolsCheck = new QCheckBox(tr("Format photo TIFF/JPG et métadonnées (laboratoire)"), featuresGroup);
    auto refreshFeatureChecks = [this, featureClassesCheck, featurePinCheck, featureTimerCheck,
                                  featureAidsCheck, featureLabToolsCheck]() {
        featureClassesCheck->setChecked(m_settings->featureClassesEnabled());
        featurePinCheck->setChecked(m_settings->featurePinLockEnabled());
        featureTimerCheck->setChecked(m_settings->featureLabTimerEnabled());
        featureAidsCheck->setChecked(m_settings->featureLearningAidsEnabled());
        featureLabToolsCheck->setChecked(m_settings->featureLabToolsEnabled());
    };
    refreshFeatureChecks();
    featuresLayout->addWidget(featureClassesCheck);
    featuresLayout->addWidget(featurePinCheck);
    featuresLayout->addWidget(featureTimerCheck);
    featuresLayout->addWidget(featureAidsCheck);
    featuresLayout->addWidget(featureLabToolsCheck);

    auto *featuresNote = new QLabel(
        tr("Les sections correspondantes apparaissent ou disparaissent à la réouverture de "
           "cette fenêtre."),
        featuresGroup);
    featuresNote->setWordWrap(true);
    featuresLayout->addWidget(featuresNote);

    connect(editionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this, editionCombo, refreshFeatureChecks](int index) {
                m_settings->setAppMode(editionCombo->itemData(index).toString());
                refreshFeatureChecks();
                setWindowTitle(m_settings->appMode() == QLatin1String("school")
                    ? tr("Mode professeur") : tr("Réglages avancés"));
            });
    connect(featureClassesCheck, &QCheckBox::toggled, m_settings, &AppSettings::setFeatureClassesEnabled);
    connect(featurePinCheck, &QCheckBox::toggled, m_settings, &AppSettings::setFeaturePinLockEnabled);
    connect(featureTimerCheck, &QCheckBox::toggled, m_settings, &AppSettings::setFeatureLabTimerEnabled);
    connect(featureAidsCheck, &QCheckBox::toggled, m_settings, &AppSettings::setFeatureLearningAidsEnabled);
    connect(featureLabToolsCheck, &QCheckBox::toggled, m_settings, &AppSettings::setFeatureLabToolsEnabled);

    leftColumn->addWidget(editionGroup);
    leftColumn->addWidget(featuresGroup);

    auto *cameraGroup = new QGroupBox(tr("Caméra"), scrollContent);
    auto *cameraForm = new QFormLayout(cameraGroup);

    m_autoExposureCheck = new QCheckBox(tr("Exposition automatique"), cameraGroup);
    m_autoExposureCheck->setChecked(backend->autoExposure());
    cameraForm->addRow(m_autoExposureCheck);

    m_exposureSlider = new QSlider(Qt::Horizontal, cameraGroup);
    m_exposureSlider->setRange(0, 100);
    m_exposureSlider->setValue(backend->exposure());
    m_exposureSlider->setEnabled(!backend->autoExposure());

    m_exposureValueLabel = new QLabel(QString::number(backend->exposure()), cameraGroup);
    m_exposureValueLabel->setMinimumWidth(widthForWidestText(m_exposureValueLabel, QStringLiteral("100"), 16));
    m_exposureValueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    auto *exposureRow = new QWidget(cameraGroup);
    auto *exposureRowLayout = new QHBoxLayout(exposureRow);
    exposureRowLayout->setContentsMargins(0, 0, 0, 0);
    exposureRowLayout->addWidget(m_exposureSlider, 1);
    exposureRowLayout->addWidget(m_exposureValueLabel);
    cameraForm->addRow(tr("Exposition manuelle"), exposureRow);

    m_autoWhiteBalanceCheck = new QCheckBox(tr("Balance des blancs automatique"), cameraGroup);
    m_autoWhiteBalanceCheck->setChecked(backend->autoWhiteBalance());
    cameraForm->addRow(m_autoWhiteBalanceCheck);

    m_gainSlider = new QSlider(Qt::Horizontal, cameraGroup);
    m_gainSlider->setRange(0, 100);
    m_gainSlider->setValue(backend->gain());

    m_gainValueLabel = new QLabel(QString::number(backend->gain()), cameraGroup);
    m_gainValueLabel->setMinimumWidth(widthForWidestText(m_gainValueLabel, QStringLiteral("100"), 16));
    m_gainValueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    auto *gainRow = new QWidget(cameraGroup);
    auto *gainRowLayout = new QHBoxLayout(gainRow);
    gainRowLayout->setContentsMargins(0, 0, 0, 0);
    gainRowLayout->addWidget(m_gainSlider, 1);
    gainRowLayout->addWidget(m_gainValueLabel);
    cameraForm->addRow(tr("Gain"), gainRow);

    m_resolutionCombo = new QComboBox(cameraGroup);
    const QVector<QSize> resolutions = backend->supportedResolutions();
    for (const QSize &size : resolutions)
        m_resolutionCombo->addItem(QStringLiteral("%1 x %2").arg(size.width()).arg(size.height()), size);
    const int currentIndex = m_resolutionCombo->findData(backend->currentResolution());
    if (currentIndex >= 0)
        m_resolutionCombo->setCurrentIndex(currentIndex);
    // Same fixed-pixel-guess problem as the spin boxes below: a QComboBox's
    // own sizeHint() doesn't always leave room for the dropdown arrow at
    // every DPI/font, so the widest resolution string ("2592 x 1944") was
    // getting clipped on some displays.
    m_resolutionCombo->setMinimumWidth(widthForWidestText(m_resolutionCombo, QStringLiteral("2592 x 1944"), 40));
    cameraForm->addRow(tr("Résolution"), m_resolutionCombo);

    leftColumn->addWidget(cameraGroup);

    auto *displayGroup = new QGroupBox(tr("Affichage"), scrollContent);
    auto *displayLayout = new QVBoxLayout(displayGroup);
    m_monochromeCheck = new QCheckBox(tr("Affichage noir et blanc"), displayGroup);
    m_monochromeCheck->setChecked(m_settings->monochromeDisplay());
    displayLayout->addWidget(m_monochromeCheck);
    m_lightThemeCheck = new QCheckBox(tr("Thème clair (au lieu de sombre)"), displayGroup);
    m_lightThemeCheck->setChecked(m_settings->lightTheme());
    displayLayout->addWidget(m_lightThemeCheck);
    m_gridCheck = new QCheckBox(tr("Grille de cadrage (règle des tiers)"), displayGroup);
    m_gridCheck->setChecked(m_settings->showGrid());
    displayLayout->addWidget(m_gridCheck);
    m_focusIndicatorCheck = new QCheckBox(tr("Indicateur de netteté"), displayGroup);
    m_focusIndicatorCheck->setChecked(m_settings->showFocusIndicator());
    displayLayout->addWidget(m_focusIndicatorCheck);

    m_soundNotificationsCheck = new QCheckBox(
        tr("Bip sonore (fin d'enregistrement, minuteur terminé)"), displayGroup);
    m_soundNotificationsCheck->setChecked(m_settings->soundNotificationsEnabled());
    displayLayout->addWidget(m_soundNotificationsCheck);

    auto *languageRow = new QWidget(displayGroup);
    auto *languageRowLayout = new QHBoxLayout(languageRow);
    languageRowLayout->setContentsMargins(0, 0, 0, 0);
    auto *languageLabel = new QLabel(tr("Langue de l'interface"), languageRow);
    m_languageCombo = new QComboBox(languageRow);
    m_languageCombo->addItem(QStringLiteral("Français"), QStringLiteral("fr"));
    m_languageCombo->addItem(QStringLiteral("English"), QStringLiteral("en"));
    m_languageCombo->setCurrentIndex(m_settings->uiLanguage() == QLatin1String("en") ? 1 : 0);
    languageRowLayout->addWidget(languageLabel, 1);
    languageRowLayout->addWidget(m_languageCombo);
    displayLayout->addWidget(languageRow);

    m_scaleBarCheck = new QCheckBox(tr("Afficher une échelle de mesure"), displayGroup);
    m_scaleBarCheck->setChecked(m_settings->showScaleBar());
    displayLayout->addWidget(m_scaleBarCheck);

    auto *scaleBarRow = new QWidget(displayGroup);
    auto *scaleBarRowLayout = new QHBoxLayout(scaleBarRow);
    scaleBarRowLayout->setContentsMargins(24, 0, 0, 0);
    auto *scaleBarLabel = new QLabel(tr("Étalonnage : µm pour 100 px de l'image"), scaleBarRow);
    m_scaleBarCalibrationSpin = new QDoubleSpinBox(scaleBarRow);
    m_scaleBarCalibrationSpin->setRange(0.0, 100000.0);
    m_scaleBarCalibrationSpin->setDecimals(1);
    m_scaleBarCalibrationSpin->setValue(m_settings->scaleBarMicronsPer100Px());
    m_scaleBarCalibrationSpin->setMinimumWidth(
        widthForWidestText(m_scaleBarCalibrationSpin, QStringLiteral("100000.0"), 40));
    auto *calibrationWizardButton = new QPushButton(tr("Étalonner avec la caméra..."), scaleBarRow);
    scaleBarRowLayout->addWidget(scaleBarLabel, 1);
    scaleBarRowLayout->addWidget(m_scaleBarCalibrationSpin);
    displayLayout->addWidget(scaleBarRow);
    auto *calibrationButtonRow = new QWidget(displayGroup);
    auto *calibrationButtonLayout = new QHBoxLayout(calibrationButtonRow);
    calibrationButtonLayout->setContentsMargins(24, 4, 0, 0);
    calibrationButtonLayout->addWidget(calibrationWizardButton);
    calibrationButtonLayout->addStretch();
    displayLayout->addWidget(calibrationButtonRow);

    leftColumn->addWidget(displayGroup);
    leftColumn->addStretch();

    auto *timeLapseGroup = new QGroupBox(tr("Accéléré (time-lapse)"), scrollContent);
    auto *timeLapseLayout = new QVBoxLayout(timeLapseGroup);
    m_timeLapseCheck = new QCheckBox(tr("Activer la capture accélérée"), timeLapseGroup);
    m_timeLapseCheck->setChecked(m_settings->timeLapseEnabled());
    timeLapseLayout->addWidget(m_timeLapseCheck);

    auto *timeLapseRow = new QWidget(timeLapseGroup);
    auto *timeLapseRowLayout = new QHBoxLayout(timeLapseRow);
    timeLapseRowLayout->setContentsMargins(24, 0, 0, 0);
    auto *timeLapseLabel = new QLabel(tr("Intervalle"), timeLapseRow);
    m_timeLapseIntervalSpin = new QSpinBox(timeLapseRow);
    m_timeLapseIntervalSpin->setRange(5, 3600);
    m_timeLapseIntervalSpin->setSuffix(QStringLiteral(" s"));
    m_timeLapseIntervalSpin->setMinimumWidth(widthForWidestText(m_timeLapseIntervalSpin, QStringLiteral("3600 s"), 40));
    m_timeLapseIntervalSpin->setValue(m_settings->timeLapseIntervalSeconds());
    timeLapseRowLayout->addWidget(timeLapseLabel, 1);
    timeLapseRowLayout->addWidget(m_timeLapseIntervalSpin);
    timeLapseLayout->addWidget(timeLapseRow);

    rightColumn->addWidget(timeLapseGroup);

    auto *storageGroup = new QGroupBox(tr("Enregistrement"), scrollContent);
    auto *storageForm = new QFormLayout(storageGroup);

    auto *folderRow = new QWidget(storageGroup);
    auto *folderLayout = new QHBoxLayout(folderRow);
    folderLayout->setContentsMargins(0, 0, 0, 0);
    m_folderEdit = new QLineEdit(m_settings->captureFolder(), folderRow);
    m_folderEdit->setReadOnly(true);
    auto *browseButton = new QPushButton(tr("Parcourir..."), folderRow);
    folderLayout->addWidget(m_folderEdit, 1);
    folderLayout->addWidget(browseButton);
    storageForm->addRow(tr("Dossier"), folderRow);

    if (m_settings->featureLabToolsEnabled()) {
        // Lab-friendly photo format choice: PNG (lossless, default), TIFF
        // (the usual archival/analysis format in lab pipelines), JPG (small).
        auto *photoFormatCombo = new QComboBox(storageGroup);
        photoFormatCombo->addItem(tr("PNG (sans perte)"), QStringLiteral("png"));
        photoFormatCombo->addItem(tr("TIFF (archivage/analyse)"), QStringLiteral("tiff"));
        photoFormatCombo->addItem(tr("JPG (léger)"), QStringLiteral("jpg"));
        const int formatIndex = photoFormatCombo->findData(m_settings->photoFormat());
        photoFormatCombo->setCurrentIndex(formatIndex >= 0 ? formatIndex : 0);
        connect(photoFormatCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
                [this, photoFormatCombo](int index) {
                    m_settings->setPhotoFormat(photoFormatCombo->itemData(index).toString());
                });
        storageForm->addRow(tr("Format photo"), photoFormatCombo);

        auto *metadataCheck = new QCheckBox(
            tr("Enregistrer un fichier de métadonnées (.txt) avec chaque photo"), storageGroup);
        metadataCheck->setToolTip(tr(
            "Date, résolution, exposition, gain, étalonnage de l'échelle, nom du microscope — "
            "à côté de chaque photo, pour la traçabilité des observations."));
        metadataCheck->setChecked(m_settings->saveCaptureMetadata());
        connect(metadataCheck, &QCheckBox::toggled, m_settings, &AppSettings::setSaveCaptureMetadata);
        storageForm->addRow(QString(), metadataCheck);
    }

    // The user names their own microscope; shown in the top bar title and
    // the info panel instead of a hardcoded model.
    auto *microscopeNameEdit = new QLineEdit(m_settings->microscopeName(), storageGroup);
    microscopeNameEdit->setPlaceholderText(tr("ex : OMAX 83S, Bresser Erudit..."));
    connect(microscopeNameEdit, &QLineEdit::textChanged, m_settings, &AppSettings::setMicroscopeName);
    storageForm->addRow(tr("Nom du microscope"), microscopeNameEdit);

    rightColumn->addWidget(storageGroup);

    if (m_settings->featureClassesEnabled()) {
        auto *classesGroup = new QGroupBox(tr("Classes et groupes"), scrollContent);
        auto *classesLayout = new QVBoxLayout(classesGroup);
        auto *manageClassesButton = new QPushButton(tr("Gérer les classes et groupes..."), classesGroup);
        auto *overviewButton = new QPushButton(tr("Vue d'ensemble des groupes..."), classesGroup);
        classesLayout->addWidget(manageClassesButton);
        classesLayout->addWidget(overviewButton);
        rightColumn->addWidget(classesGroup);
        connect(manageClassesButton, &QPushButton::clicked, this, &TeacherPanel::onManageClasses);
        connect(overviewButton, &QPushButton::clicked, this, &TeacherPanel::onOpenOverview);
    }

    if (m_settings->featureLabTimerEnabled()) {
        auto *timerGroup = new QGroupBox(tr("Minuteur d'observation"), scrollContent);
        auto *timerLayout = new QHBoxLayout(timerGroup);
        auto *timerLabel = new QLabel(tr("Durée"), timerGroup);
        m_labTimerMinutesSpin = new QSpinBox(timerGroup);
        m_labTimerMinutesSpin->setRange(1, 180);
        m_labTimerMinutesSpin->setSuffix(QStringLiteral(" min"));
        m_labTimerMinutesSpin->setValue(10);
        m_labTimerMinutesSpin->setMinimumWidth(
            widthForWidestText(m_labTimerMinutesSpin, QStringLiteral("180 min"), 40));
        auto *labTimerStartButton = new QPushButton(tr("Démarrer"), timerGroup);
        auto *labTimerStopButton = new QPushButton(tr("Arrêter"), timerGroup);
        timerLayout->addWidget(timerLabel);
        timerLayout->addWidget(m_labTimerMinutesSpin);
        timerLayout->addWidget(labTimerStartButton);
        timerLayout->addWidget(labTimerStopButton);
        timerLayout->addStretch();
        rightColumn->addWidget(timerGroup);
        connect(labTimerStartButton, &QPushButton::clicked, this, [this]() {
            emit labTimerStartRequested(m_labTimerMinutesSpin->value());
        });
        connect(labTimerStopButton, &QPushButton::clicked, this, &TeacherPanel::labTimerStopRequested);
    }

    auto *backupGroup = new QGroupBox(tr("Sauvegarde automatique"), scrollContent);
    auto *backupLayout = new QVBoxLayout(backupGroup);
    m_autoBackupCheck = new QCheckBox(tr("Activer la sauvegarde automatique"), backupGroup);
    m_autoBackupCheck->setChecked(m_settings->autoBackupEnabled());
    backupLayout->addWidget(m_autoBackupCheck);

    auto *backupDestRow = new QWidget(backupGroup);
    auto *backupDestLayout = new QHBoxLayout(backupDestRow);
    backupDestLayout->setContentsMargins(0, 0, 0, 0);
    m_autoBackupDestEdit = new QLineEdit(m_settings->autoBackupDestination(), backupDestRow);
    m_autoBackupDestEdit->setReadOnly(true);
    m_autoBackupDestEdit->setPlaceholderText(tr("Aucune destination choisie"));
    auto *chooseBackupDestButton = new QPushButton(tr("Choisir..."), backupDestRow);
    backupDestLayout->addWidget(m_autoBackupDestEdit, 1);
    backupDestLayout->addWidget(chooseBackupDestButton);
    backupLayout->addWidget(backupDestRow);

    auto *backupIntervalRow = new QWidget(backupGroup);
    auto *backupIntervalLayout = new QHBoxLayout(backupIntervalRow);
    backupIntervalLayout->setContentsMargins(0, 0, 0, 0);
    auto *backupIntervalLabel = new QLabel(tr("Tous les"), backupIntervalRow);
    m_autoBackupIntervalSpin = new QSpinBox(backupIntervalRow);
    m_autoBackupIntervalSpin->setRange(1, 90);
    m_autoBackupIntervalSpin->setSuffix(QStringLiteral(" jours"));
    m_autoBackupIntervalSpin->setValue(m_settings->autoBackupIntervalDays());
    m_autoBackupIntervalSpin->setMinimumWidth(
        widthForWidestText(m_autoBackupIntervalSpin, QStringLiteral("90 jours"), 40));
    auto *backupNowButton = new QPushButton(tr("Sauvegarder maintenant"), backupIntervalRow);
    backupIntervalLayout->addWidget(backupIntervalLabel);
    backupIntervalLayout->addWidget(m_autoBackupIntervalSpin);
    backupIntervalLayout->addStretch();
    backupIntervalLayout->addWidget(backupNowButton);
    backupLayout->addWidget(backupIntervalRow);

    m_autoBackupLastLabel = new QLabel(backupGroup);
    backupLayout->addWidget(m_autoBackupLastLabel);
    refreshAutoBackupLastLabel();

    rightColumn->addWidget(backupGroup);

    if (m_settings->featurePinLockEnabled()) {
        auto *lockGroup = new QGroupBox(tr("Verrouillage"), scrollContent);
        auto *lockLayout = new QVBoxLayout(lockGroup);
        m_studentLockCheck = new QCheckBox(
            tr("Verrouiller le mode élève (code PIN requis pour fermer l'application)"), lockGroup);
        m_studentLockCheck->setChecked(m_settings->studentModeLocked());
        lockLayout->addWidget(m_studentLockCheck);
        rightColumn->addWidget(lockGroup);
        connect(m_studentLockCheck, &QCheckBox::toggled, this, &TeacherPanel::onStudentLockToggled);
    }

    auto *securityGroup = new QGroupBox(tr("Sécurité et veille"), scrollContent);
    auto *securityForm = new QFormLayout(securityGroup);

    if (m_settings->featurePinLockEnabled()) {
        auto *pinRow = new QWidget(securityGroup);
        auto *pinLayout = new QHBoxLayout(pinRow);
        pinLayout->setContentsMargins(0, 0, 0, 0);
        m_newPinEdit = new QLineEdit(pinRow);
        m_newPinEdit->setEchoMode(QLineEdit::Password);
        m_newPinEdit->setPlaceholderText(tr("Nouveau PIN"));
        m_confirmPinEdit = new QLineEdit(pinRow);
        m_confirmPinEdit->setEchoMode(QLineEdit::Password);
        m_confirmPinEdit->setPlaceholderText(tr("Confirmer"));
        auto *changePinButton = new QPushButton(tr("Changer"), pinRow);
        pinLayout->addWidget(m_newPinEdit);
        pinLayout->addWidget(m_confirmPinEdit);
        pinLayout->addWidget(changePinButton);
        securityForm->addRow(tr("Code PIN"), pinRow);
        connect(changePinButton, &QPushButton::clicked, this, &TeacherPanel::onChangePin);
    }

    m_idleTimeoutSpin = new QSpinBox(securityGroup);
    m_idleTimeoutSpin->setRange(0, 60);
    m_idleTimeoutSpin->setSuffix(QStringLiteral(" min"));
    m_idleTimeoutSpin->setSpecialValueText(tr("Désactivée"));
    // Sized off the widest string it'll ever show ("Désactivée"), measured
    // in its own real font, rather than a flat guess — a fixed pixel width
    // kept clipping on some displays regardless of how generous it was.
    m_idleTimeoutSpin->setMinimumWidth(widthForWidestText(m_idleTimeoutSpin, tr("Désactivée"), 40));
    m_idleTimeoutSpin->setValue(m_settings->idleTimeoutMinutes());
    securityForm->addRow(tr("Veille après"), m_idleTimeoutSpin);

    rightColumn->addWidget(securityGroup);
    rightColumn->addStretch();

    auto *closeButton = new QPushButton(tr("Fermer"), this);
    root->addWidget(closeButton);

    connect(m_autoExposureCheck, &QCheckBox::toggled, this, &TeacherPanel::onAutoExposureToggled);
    connect(m_autoWhiteBalanceCheck, &QCheckBox::toggled, this, &TeacherPanel::onAutoWhiteBalanceToggled);
    connect(m_exposureSlider, &QSlider::valueChanged, this, &TeacherPanel::onExposureSliderChanged);
    connect(m_gainSlider, &QSlider::valueChanged, this, &TeacherPanel::onGainSliderChanged);
    connect(m_resolutionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &TeacherPanel::onResolutionChanged);
    connect(m_monochromeCheck, &QCheckBox::toggled, m_settings, &AppSettings::setMonochromeDisplay);
    connect(m_lightThemeCheck, &QCheckBox::toggled, m_settings, &AppSettings::setLightTheme);
    connect(m_soundNotificationsCheck, &QCheckBox::toggled, m_settings, &AppSettings::setSoundNotificationsEnabled);
    connect(m_languageCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &TeacherPanel::onLanguageChanged);
    connect(m_gridCheck, &QCheckBox::toggled, m_settings, &AppSettings::setShowGrid);
    connect(m_focusIndicatorCheck, &QCheckBox::toggled, m_settings, &AppSettings::setShowFocusIndicator);
    connect(m_scaleBarCheck, &QCheckBox::toggled, m_settings, &AppSettings::setShowScaleBar);
    connect(m_scaleBarCalibrationSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &TeacherPanel::onScaleBarCalibrationChanged);
    connect(calibrationWizardButton, &QPushButton::clicked, this, &TeacherPanel::onOpenCalibrationWizard);
    connect(m_timeLapseCheck, &QCheckBox::toggled, m_settings, &AppSettings::setTimeLapseEnabled);
    connect(m_timeLapseIntervalSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            m_settings, &AppSettings::setTimeLapseIntervalSeconds);
    connect(browseButton, &QPushButton::clicked, this, &TeacherPanel::onBrowseFolder);
    connect(m_autoBackupCheck, &QCheckBox::toggled, m_settings, &AppSettings::setAutoBackupEnabled);
    connect(chooseBackupDestButton, &QPushButton::clicked, this, &TeacherPanel::onChooseBackupDestination);
    connect(m_autoBackupIntervalSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            m_settings, &AppSettings::setAutoBackupIntervalDays);
    connect(backupNowButton, &QPushButton::clicked, this, &TeacherPanel::onAutoBackupNow);
    connect(m_idleTimeoutSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &TeacherPanel::onIdleTimeoutChanged);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);

    refreshCameraInfo();
}

void TeacherPanel::refreshCameraInfo()
{
    m_cameraInfoLabel->setText(m_backend->isOpen()
        ? tr("Backend : %1 — connectée").arg(m_backend->backendName())
        : tr("Backend : %1 — aucune caméra détectée").arg(m_backend->backendName()));
}

void TeacherPanel::onAutoExposureToggled(bool checked)
{
    m_backend->setAutoExposure(checked);
    m_exposureSlider->setEnabled(!checked);
}

void TeacherPanel::onAutoWhiteBalanceToggled(bool checked)
{
    m_backend->setAutoWhiteBalance(checked);
}

void TeacherPanel::onExposureSliderChanged(int value)
{
    m_backend->setExposure(value);
    m_exposureValueLabel->setText(QString::number(value));
}

void TeacherPanel::onGainSliderChanged(int value)
{
    m_backend->setGain(value);
    m_gainValueLabel->setText(QString::number(value));
}

void TeacherPanel::onResolutionChanged(int index)
{
    const QSize size = m_resolutionCombo->itemData(index).toSize();
    if (!size.isEmpty())
        m_backend->setResolution(size);
}

void TeacherPanel::onBrowseFolder()
{
    const QString folder = QFileDialog::getExistingDirectory(this, tr("Dossier d'enregistrement"),
                                                               m_folderEdit->text());
    if (folder.isEmpty())
        return;

    m_folderEdit->setText(folder);
    m_settings->setCaptureFolder(folder);
}

void TeacherPanel::onManageClasses()
{
    ClassManagerDialog dialog(m_settings, this);
    dialog.exec();
}

void TeacherPanel::onChangePin()
{
    const QString newPin = m_newPinEdit->text();
    if (newPin.length() < 4) {
        QMessageBox::warning(this, tr("Code PIN"), tr("Le code PIN doit contenir au moins 4 chiffres."));
        return;
    }
    if (newPin != m_confirmPinEdit->text()) {
        QMessageBox::warning(this, tr("Code PIN"), tr("Les deux codes PIN ne correspondent pas."));
        return;
    }

    m_settings->setTeacherPin(newPin);
    m_newPinEdit->clear();
    m_confirmPinEdit->clear();
    QMessageBox::information(this, tr("Code PIN"), tr("Code PIN mis à jour."));
}

void TeacherPanel::onStudentLockToggled(bool checked)
{
    // Locking is meaningless without a PIN to unlock with — rather than let
    // the checkbox silently do nothing (the Windows 7 bug report: closing
    // worked with no password because no PIN had ever been set), require
    // one to be created right here before the lock takes effect.
    if (checked && m_settings->teacherPinHash().isEmpty()) {
        bool ok = false;
        const QString newPin = QInputDialog::getText(
            this, tr("Code PIN requis"),
            tr("Verrouiller la fermeture exige un code PIN professeur.\n"
               "Choisissez un code PIN (4 chiffres minimum) :"),
            QLineEdit::Password, QString(), &ok);
        if (!ok) {
            m_studentLockCheck->setChecked(false);
            return;
        }
        if (newPin.length() < 4) {
            QMessageBox::warning(this, tr("Code PIN"), tr("Le code PIN doit contenir au moins 4 chiffres."));
            m_studentLockCheck->setChecked(false);
            return;
        }
        bool okConfirm = false;
        const QString confirmPin = QInputDialog::getText(this, tr("Confirmer le code PIN"),
                                                           tr("Confirmez le code PIN :"),
                                                           QLineEdit::Password, QString(), &okConfirm);
        if (!okConfirm || confirmPin != newPin) {
            QMessageBox::warning(this, tr("Code PIN"), tr("Les deux codes PIN ne correspondent pas."));
            m_studentLockCheck->setChecked(false);
            return;
        }
        m_settings->setTeacherPin(newPin);
    }
    m_settings->setStudentModeLocked(checked);
}

void TeacherPanel::onLanguageChanged(int index)
{
    const QString language = m_languageCombo->itemData(index).toString();
    if (language == m_settings->uiLanguage())
        return;
    m_settings->setUiLanguage(language);
    // Every widget in this app sets its text once at construction (no
    // Qt Linguist-style retranslateUi()), so the new language only takes
    // effect after a full restart — simpler and far less code than wiring
    // live retranslation into every dialog.
    QMessageBox::information(this, tr("Langue de l'interface"),
                              tr("Redémarrez E-Lab 700 pour appliquer le changement de langue."));
}

void TeacherPanel::onIdleTimeoutChanged(int minutes)
{
    m_settings->setIdleTimeoutMinutes(minutes);
}

void TeacherPanel::onScaleBarCalibrationChanged(double value)
{
    m_settings->setScaleBarMicronsPer100Px(value);
}

void TeacherPanel::onOpenCalibrationWizard()
{
    const QImage snapshot = m_frameProvider ? m_frameProvider() : QImage();
    if (snapshot.isNull()) {
        QMessageBox::warning(this, tr("Étalonnage"),
                              tr("Aucune image de caméra disponible pour le moment."));
        return;
    }

    ScaleCalibrationDialog dialog(snapshot, m_settings, this);
    if (dialog.exec() == QDialog::Accepted) {
        m_scaleBarCalibrationSpin->setValue(m_settings->scaleBarMicronsPer100Px());
        m_scaleBarCheck->setChecked(m_settings->showScaleBar());
    }
}

void TeacherPanel::onOpenOverview()
{
    TeacherOverviewDialog dialog(m_settings, this);
    dialog.exec();
}

void TeacherPanel::onChooseBackupDestination()
{
    const QString folder = QFileDialog::getExistingDirectory(this, tr("Destination de la sauvegarde"),
                                                               m_autoBackupDestEdit->text());
    if (folder.isEmpty())
        return;

    m_autoBackupDestEdit->setText(folder);
    m_settings->setAutoBackupDestination(folder);
}

void TeacherPanel::onAutoBackupNow()
{
    const QString destinationRoot = m_settings->autoBackupDestination();
    if (destinationRoot.isEmpty()) {
        QMessageBox::warning(this, tr("Sauvegarde"),
                              tr("Choisissez d'abord une destination de sauvegarde."));
        return;
    }

    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
    const QString destination = QDir(destinationRoot).filePath(tr("E-Lab700_Sauvegarde_%1").arg(timestamp));

    if (!FileUtils::copyFolderRecursively(m_settings->captureFolder(), destination, GalleryModel::trashFolderName())) {
        QMessageBox::warning(this, tr("Sauvegarde"),
                              tr("La sauvegarde a échoué ou est incomplète. Vérifiez l'espace disponible."));
        return;
    }

    m_settings->setLastAutoBackupAt(QDateTime::currentDateTime());
    refreshAutoBackupLastLabel();
    QMessageBox::information(this, tr("Sauvegarde"),
                              tr("Sauvegarde effectuée :\n%1").arg(destination));
}

void TeacherPanel::refreshAutoBackupLastLabel()
{
    const QDateTime last = m_settings->lastAutoBackupAt();
    m_autoBackupLastLabel->setText(last.isValid()
        ? tr("Dernière sauvegarde : %1").arg(last.toString(QStringLiteral("dd/MM/yyyy HH:mm")))
        : tr("Dernière sauvegarde : jamais"));
}
