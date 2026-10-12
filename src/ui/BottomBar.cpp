#include "BottomBar.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QStyle>
#include <QVariant>

#include "IconButton.h"

namespace {

IconButton *makeBigButton(LineIcon icon, const QString &text, const QString &objectName)
{
    auto *button = new IconButton(icon, text, IconButton::Layout::IconAboveText);
    button->setObjectName(objectName);
    button->setMinimumSize(140, 90);
    QFont font = button->font();
    font.setPointSize(13);
    font.setBold(true);
    button->setFont(font);
    return button;
}

QPushButton *makeZoomButton(const QString &text, const QString &objectName)
{
    auto *button = new QPushButton(text);
    button->setObjectName(objectName);
    button->setFixedSize(44, 44);
    button->setCursor(Qt::PointingHandCursor);
    QFont font = button->font();
    font.setPointSize(16);
    font.setBold(true);
    button->setFont(font);
    return button;
}

IconButton *makeSecondaryButton(LineIcon icon, const QString &text)
{
    auto *button = new IconButton(icon, text, IconButton::Layout::IconBesideText);
    button->setFixedHeight(44);
    return button;
}

} // namespace

BottomBar::BottomBar(QWidget *parent) : QWidget(parent)
{
    setObjectName("BottomBar");

    m_photoButton = makeBigButton(LineIcon::Camera, tr("Photo"), "photoButton");
    m_videoButton = makeBigButton(LineIcon::Video, tr("Vidéo"), "videoButton");
    m_autoButton = makeBigButton(LineIcon::Sparkle, tr("Auto"), "autoButton");
    m_galleryButton = makeBigButton(LineIcon::Gallery, tr("Galerie"), "galleryButton");
    m_fullscreenButton = makeBigButton(LineIcon::Fullscreen, tr("Plein écran"), "fullscreenButton");

    m_zoomOutButton = makeZoomButton(QStringLiteral("−"), "zoomOutButton");
    m_zoomInButton = makeZoomButton(QStringLiteral("+"), "zoomInButton");
    // A button (not a plain label) so clicking it resets the zoom to 100%
    // directly, instead of needing several taps on "−".
    m_zoomLabel = new QPushButton(QStringLiteral("100%"), this);
    m_zoomLabel->setObjectName("zoomLabel");
    m_zoomLabel->setFlat(true);
    m_zoomLabel->setCursor(Qt::PointingHandCursor);
    m_zoomLabel->setToolTip(tr("Revenir à 100%"));
    m_zoomLabel->setFixedWidth(48);

    // Three equal-stretch grid columns (zoom | main buttons | empty mirror)
    // guarantee the main buttons sit at the true horizontal center of the
    // bar regardless of how wide the zoom cluster or the buttons themselves
    // turn out to be — a fixed-width spacer approach doesn't survive style
    // or content changes as safely as this does.
    auto *zoomContainer = new QWidget(this);
    auto *zoomLayout = new QHBoxLayout(zoomContainer);
    zoomLayout->setContentsMargins(0, 0, 0, 0);
    zoomLayout->setSpacing(8);
    zoomLayout->addWidget(m_zoomOutButton);
    zoomLayout->addWidget(m_zoomLabel);
    zoomLayout->addWidget(m_zoomInButton);

    auto *buttonsContainer = new QWidget(this);
    auto *buttonsLayout = new QHBoxLayout(buttonsContainer);
    buttonsLayout->setContentsMargins(0, 0, 0, 0);
    buttonsLayout->setSpacing(20);
    buttonsLayout->addWidget(m_photoButton);
    buttonsLayout->addWidget(m_videoButton);
    buttonsLayout->addWidget(m_autoButton);
    // Counting/measuring analysis tools — common to every edition (the
    // grand-public and school editions analyze images too, not just the
    // lab edition).
    auto *analysisButton = makeBigButton(LineIcon::Analysis, tr("Analyse"), "galleryButton");
    buttonsLayout->addWidget(analysisButton);
    connect(analysisButton, &QPushButton::clicked, this, &BottomBar::analysisRequested);
    buttonsLayout->addWidget(m_galleryButton);
    buttonsLayout->addWidget(m_fullscreenButton);

    // Bottom-right, next to Plein écran — the diagram/glossary learning
    // aids, in the column that used to just mirror the left zoom column's
    // stretch for centering purposes.
    m_diagramButton = makeSecondaryButton(LineIcon::Microscope, tr("Schéma"));
    m_glossaryButton = makeSecondaryButton(LineIcon::Book, tr("Glossaire"));
    auto *learningAidsContainer = new QWidget(this);
    auto *learningAidsLayout = new QHBoxLayout(learningAidsContainer);
    learningAidsLayout->setContentsMargins(0, 0, 0, 0);
    learningAidsLayout->setSpacing(8);
    learningAidsLayout->addWidget(m_diagramButton);
    learningAidsLayout->addWidget(m_glossaryButton);

    auto *layout = new QGridLayout(this);
    layout->setContentsMargins(24, 16, 24, 16);
    layout->setColumnStretch(0, 1);
    layout->setColumnStretch(1, 0);
    layout->setColumnStretch(2, 1);
    layout->addWidget(zoomContainer, 0, 0, Qt::AlignLeft | Qt::AlignVCenter);
    layout->addWidget(buttonsContainer, 0, 1, Qt::AlignCenter);
    layout->addWidget(learningAidsContainer, 0, 2, Qt::AlignRight | Qt::AlignVCenter);
    // Both side columns must start from the same width, or the wider one
    // pushes the main buttons off-center even with equal stretch.
    const int sideWidth = qMax(zoomContainer->sizeHint().width(), learningAidsContainer->sizeHint().width());
    layout->setColumnMinimumWidth(0, sideWidth);
    layout->setColumnMinimumWidth(2, sideWidth);

    connect(m_photoButton, &QPushButton::clicked, this, &BottomBar::photoRequested);
    connect(m_videoButton, &QPushButton::clicked, this, &BottomBar::videoToggleRequested);
    connect(m_autoButton, &QPushButton::clicked, this, &BottomBar::autoRequested);
    connect(m_galleryButton, &QPushButton::clicked, this, &BottomBar::galleryRequested);
    connect(m_fullscreenButton, &QPushButton::clicked, this, &BottomBar::fullscreenRequested);
    connect(m_zoomOutButton, &QPushButton::clicked, this, &BottomBar::zoomOutRequested);
    connect(m_zoomInButton, &QPushButton::clicked, this, &BottomBar::zoomInRequested);
    connect(m_zoomLabel, &QPushButton::clicked, this, &BottomBar::zoomResetRequested);
    connect(m_diagramButton, &QPushButton::clicked, this, &BottomBar::diagramRequested);
    connect(m_glossaryButton, &QPushButton::clicked, this, &BottomBar::glossaryRequested);
}

void BottomBar::setZoomPercent(int percent)
{
    m_zoomLabel->setText(QStringLiteral("%1%").arg(percent));
}

void BottomBar::setLearningAidsVisible(bool visible)
{
    m_diagramButton->setVisible(visible);
    m_glossaryButton->setVisible(visible);
}

void BottomBar::setRecording(bool recording)
{
    m_videoButton->setText(recording ? tr("Stop") : tr("Vidéo"));
    m_videoButton->setLineIcon(recording ? LineIcon::Stop : LineIcon::Video);
    m_videoButton->setProperty("recording", QVariant(recording));
    m_videoButton->style()->unpolish(m_videoButton);
    m_videoButton->style()->polish(m_videoButton);
}
