#include "RevealCompareWidget.h"

#include <QMouseEvent>
#include <QPainter>

RevealCompareWidget::RevealCompareWidget(QWidget *parent) : QWidget(parent)
{
    setMinimumSize(320, 240);
    setCursor(Qt::SplitHCursor);
}

void RevealCompareWidget::setImages(const QPixmap &before, const QPixmap &after)
{
    m_before = before;
    m_after = after;
    update();
}

QRect RevealCompareWidget::displayRect() const
{
    if (m_before.isNull())
        return rect();
    const QSize target = m_before.size().scaled(size(), Qt::KeepAspectRatio);
    return QRect(QPoint((width() - target.width()) / 2, (height() - target.height()) / 2), target);
}

void RevealCompareWidget::paintEvent(QPaintEvent * /*event*/)
{
    QPainter painter(this);
    painter.fillRect(rect(), QColor("#05070a"));

    if (m_before.isNull() || m_after.isNull())
        return;

    const QRect target = displayRect();
    painter.drawPixmap(target, m_before);

    const int dividerX = target.left() + static_cast<int>(target.width() * m_dividerFraction);
    painter.setClipRect(QRect(target.left(), target.top(), dividerX - target.left(), target.height()));
    painter.drawPixmap(target, m_after);
    painter.setClipping(false);

    QPen pen(Qt::white);
    pen.setWidth(3);
    painter.setPen(pen);
    painter.drawLine(dividerX, target.top(), dividerX, target.bottom());

    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(Qt::white);
    painter.drawEllipse(QPoint(dividerX, target.center().y()), 10, 10);
}

void RevealCompareWidget::updateDividerFromX(int x)
{
    const QRect target = displayRect();
    if (target.width() <= 0)
        return;
    const double fraction = static_cast<double>(x - target.left()) / target.width();
    m_dividerFraction = qBound(0.0, fraction, 1.0);
    update();
}

void RevealCompareWidget::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        updateDividerFromX(event->pos().x());
}

void RevealCompareWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton)
        updateDividerFromX(event->pos().x());
}
