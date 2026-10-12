#include "IconButton.h"

#include <QFontMetrics>
#include <QPainter>
#include <QStyleOptionButton>
#include <QStylePainter>

namespace {
constexpr int kAboveIconSize = 34;
constexpr int kBesideIconSize = 18;
constexpr int kGap = 8;
}

IconButton::IconButton(LineIcon icon, const QString &label, Layout layout, QWidget *parent)
    : QPushButton(label, parent), m_icon(icon), m_layout(layout)
{
    setCursor(Qt::PointingHandCursor);
}

void IconButton::setLineIcon(LineIcon icon)
{
    if (m_icon == icon)
        return;
    m_icon = icon;
    update();
}

QSize IconButton::sizeHint() const
{
    const QFontMetrics metrics(font());
    const int textWidth = metrics.horizontalAdvance(text());
    if (m_layout == Layout::IconAboveText)
        return QSize(qMax(textWidth + 32, 140), kAboveIconSize + kGap + metrics.height() + 40);
    return QSize(kBesideIconSize + kGap + textWidth + 32, 44);
}

void IconButton::paintEvent(QPaintEvent *)
{
    QStylePainter painter(this);
    QStyleOptionButton option;
    initStyleOption(&option);

    // Frame/background/hover/pressed exactly as the stylesheet defines them.
    QStyleOptionButton frame = option;
    frame.text.clear();
    frame.icon = QIcon();
    painter.drawControl(QStyle::CE_PushButton, frame);

    const QColor color = option.palette.color(QPalette::ButtonText);
    const qreal dpr = devicePixelRatioF();
    const bool above = (m_layout == Layout::IconAboveText);
    const int iconSize = above ? kAboveIconSize : kBesideIconSize;
    const QPixmap pixmap = lineIconPixmap(m_icon, color, iconSize, dpr);

    painter.setFont(font());
    painter.setPen(color);
    const QFontMetrics metrics(font());
    const QRect area = rect();

    if (above) {
        const int contentHeight = iconSize + kGap + metrics.height();
        const int top = area.top() + (area.height() - contentHeight) / 2;
        painter.drawPixmap(area.left() + (area.width() - iconSize) / 2, top, pixmap);
        painter.drawText(QRect(area.left(), top + iconSize + kGap, area.width(), metrics.height()),
                         Qt::AlignHCenter | Qt::AlignTop, text());
    } else {
        const int textWidth = metrics.horizontalAdvance(text());
        const int contentWidth = iconSize + kGap + textWidth;
        const int left = area.left() + (area.width() - contentWidth) / 2;
        painter.drawPixmap(left, area.top() + (area.height() - iconSize) / 2, pixmap);
        painter.drawText(QRect(left + iconSize + kGap, area.top(), textWidth + 2, area.height()),
                         Qt::AlignLeft | Qt::AlignVCenter, text());
    }
}
