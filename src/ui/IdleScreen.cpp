#include "IdleScreen.h"

#include <QGraphicsOpacityEffect>
#include <QLabel>
#include <QPixmap>
#include <QPropertyAnimation>
#include <QVBoxLayout>

IdleScreen::IdleScreen(QWidget *parent) : QWidget(parent)
{
    setObjectName("IdleScreen");
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor("#05070a"));
    setPalette(pal);

    m_logoLabel = new QLabel(this);
    m_logoLabel->setAlignment(Qt::AlignCenter);
    const QPixmap logo(QStringLiteral(":/branding/logo.png"));
    if (!logo.isNull())
        m_logoLabel->setPixmap(logo.scaledToWidth(360, Qt::SmoothTransformation));

    auto *layout = new QVBoxLayout(this);
    layout->addStretch();
    layout->addWidget(m_logoLabel, 0, Qt::AlignCenter);
    layout->addStretch();

    m_opacityEffect = new QGraphicsOpacityEffect(m_logoLabel);
    m_logoLabel->setGraphicsEffect(m_opacityEffect);

    // Keyframed as a full breathing cycle (dim -> bright -> dim) within a
    // single animation, so looping doesn't snap back abruptly each cycle.
    m_pulseAnimation = new QPropertyAnimation(m_opacityEffect, "opacity", this);
    m_pulseAnimation->setKeyValueAt(0.0, 0.35);
    m_pulseAnimation->setKeyValueAt(0.5, 1.0);
    m_pulseAnimation->setKeyValueAt(1.0, 0.35);
    m_pulseAnimation->setDuration(4000);
    m_pulseAnimation->setEasingCurve(QEasingCurve::InOutSine);
    m_pulseAnimation->setLoopCount(-1);
}

void IdleScreen::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);
    m_pulseAnimation->start();
}

void IdleScreen::hideEvent(QHideEvent *event)
{
    QWidget::hideEvent(event);
    m_pulseAnimation->stop();
}
