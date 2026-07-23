#include "SmoothScrollArea.h"

#include <QPropertyAnimation>
#include <QScrollBar>
#include <QWheelEvent>

void SmoothScrollArea::wheelEvent(QWheelEvent *event)
{
    QScrollBar *bar = verticalScrollBar();
    if (!bar || !bar->isVisible()) {
        QScrollArea::wheelEvent(event);
        return;
    }

    // Extend the already-animating target instead of restarting from the
    // bar's current (mid-flight) value: this is what makes a burst of rapid
    // wheel/trackpad ticks read as one continuous glide instead of a series
    // of small stop-start jumps that each cut the previous easing short.
    const bool isAnimating = m_scrollAnimation && m_scrollAnimation->state() == QAbstractAnimation::Running;
    const int baseValue = isAnimating ? m_targetValue : bar->value();

    const int step = bar->pageStep() / 3;
    const int direction = event->angleDelta().y() > 0 ? -1 : 1;
    const int target = qBound(bar->minimum(), baseValue + direction * step, bar->maximum());
    m_targetValue = target;

    if (m_scrollAnimation) {
        m_scrollAnimation->stop();
        m_scrollAnimation->deleteLater();
    }

    m_scrollAnimation = new QPropertyAnimation(bar, "value", this);
    m_scrollAnimation->setDuration(320);
    m_scrollAnimation->setStartValue(bar->value());
    m_scrollAnimation->setEndValue(target);
    m_scrollAnimation->setEasingCurve(QEasingCurve::OutCubic);
    m_scrollAnimation->start();

    event->accept();
}
