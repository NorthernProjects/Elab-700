#include "BottomBar.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QStyle>
#include <QVariant>

namespace {

QPushButton *makeBigButton(const QString &text, const QString &objectName)
{
    auto *button = new QPushButton(text);
    button->setObjectName(objectName);
    button->setMinimumSize(140, 90);
    button->setCursor(Qt::PointingHandCursor);
    QFont font = button->font();
    font.setPointSize(14);
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

} // namespace

BottomBar::BottomBar(QWidget *parent) : QWidget(parent)
{
    setObjectName("BottomBar");

    m_photoButton = makeBigButton(tr("📷\nPhoto"), "photoButton");
    m_videoButton = makeBigButton(tr("🎥\nVidéo"), "videoButton");
    m_autoButton = makeBigButton(tr("✨\nAuto"), "autoButton");
    m_galleryButton = makeBigButton(tr("🖼\nGalerie"), "galleryButton");
    m_fullscreenButton = makeBigButton(tr("⛶\nPlein écran"), "fullscreenButton");

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
    auto *analysisButton = makeBigButton(tr("📐\nAnalyse"), "galleryButton");
    buttonsLayout->addWidget(analysisButton);
    connect(analysisButton, &QPushButton::clicked, this, &BottomBar::analysisRequested);
    buttonsLayout->addWidget(m_galleryButton);
    buttonsLayout->addWidget(m_fullscreenButton);

    auto *layout = new QGridLayout(this);
    layout->setContentsMargins(24, 16, 24, 16);
    layout->setColumnStretch(0, 1);
    layout->setColumnStretch(1, 0);
    layout->setColumnStretch(2, 1);
    layout->addWidget(zoomContainer, 0, 0, Qt::AlignLeft | Qt::AlignVCenter);
    // Column 2 has no content but keeps the same stretch as column 0 so
    // column 1 (the main buttons) still lands exactly in the middle.
    layout->addWidget(buttonsContainer, 0, 1, Qt::AlignCenter);

    connect(m_photoButton, &QPushButton::clicked, this, &BottomBar::photoRequested);
    connect(m_videoButton, &QPushButton::clicked, this, &BottomBar::videoToggleRequested);
    connect(m_autoButton, &QPushButton::clicked, this, &BottomBar::autoRequested);
    connect(m_galleryButton, &QPushButton::clicked, this, &BottomBar::galleryRequested);
    connect(m_fullscreenButton, &QPushButton::clicked, this, &BottomBar::fullscreenRequested);
    connect(m_zoomOutButton, &QPushButton::clicked, this, &BottomBar::zoomOutRequested);
    connect(m_zoomInButton, &QPushButton::clicked, this, &BottomBar::zoomInRequested);
    connect(m_zoomLabel, &QPushButton::clicked, this, &BottomBar::zoomResetRequested);
}

void BottomBar::setZoomPercent(int percent)
{
    m_zoomLabel->setText(QStringLiteral("%1%").arg(percent));
}

void BottomBar::setRecording(bool recording)
{
    m_videoButton->setText(recording ? tr("🔴\nStop") : tr("🎥\nVidéo"));
    m_videoButton->setProperty("recording", QVariant(recording));
    m_videoButton->style()->unpolish(m_videoButton);
    m_videoButton->style()->polish(m_videoButton);
}
