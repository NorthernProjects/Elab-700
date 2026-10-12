#include "LineIcons.h"

#include <QPainter>
#include <QPainterPath>

namespace {

// All icons are designed on a 24x24 grid.
void drawCamera(QPainter &p)
{
    QPainterPath body;
    body.addRoundedRect(QRectF(3, 7.5, 18, 12), 3, 3);
    p.drawPath(body);
    QPainterPath bump;
    bump.moveTo(8, 7.5);
    bump.lineTo(9.6, 4.8);
    bump.lineTo(14.4, 4.8);
    bump.lineTo(16, 7.5);
    p.drawPath(bump);
    p.drawEllipse(QPointF(12, 13.5), 3.6, 3.6);
}

void drawVideo(QPainter &p)
{
    QPainterPath body;
    body.addRoundedRect(QRectF(2.5, 6.5, 13, 11), 3, 3);
    p.drawPath(body);
    QPainterPath lens;
    lens.moveTo(15.5, 10.5);
    lens.lineTo(21, 7.5);
    lens.lineTo(21, 16.5);
    lens.lineTo(15.5, 13.5);
    p.drawPath(lens);
}

void drawStop(QPainter &p)
{
    QPainterPath square;
    square.addRoundedRect(QRectF(6, 6, 12, 12), 3, 3);
    p.save();
    p.setBrush(p.pen().color());
    p.drawPath(square);
    p.restore();
}

void drawSparkle(QPainter &p)
{
    // Main four-point star plus two small ones, like the "auto/magic" glyph.
    QPainterPath star;
    star.moveTo(10, 3.5);
    star.cubicTo(10.6, 8.2, 12.4, 10, 17, 10.6);
    star.cubicTo(12.4, 11.2, 10.6, 13, 10, 17.7);
    star.cubicTo(9.4, 13, 7.6, 11.2, 3, 10.6);
    star.cubicTo(7.6, 10, 9.4, 8.2, 10, 3.5);
    p.drawPath(star);
    p.drawLine(QPointF(19, 14.5), QPointF(19, 20.5));
    p.drawLine(QPointF(16, 17.5), QPointF(22, 17.5));
    p.drawLine(QPointF(18.5, 3), QPointF(18.5, 6.5));
    p.drawLine(QPointF(16.75, 4.75), QPointF(20.25, 4.75));
}

void drawAnalysis(QPainter &p)
{
    // Set square with graduation ticks.
    QPainterPath tri;
    tri.moveTo(4.5, 3.5);
    tri.lineTo(4.5, 20.5);
    tri.lineTo(21, 20.5);
    tri.closeSubpath();
    p.drawPath(tri);
    for (int i = 0; i < 4; ++i) {
        const qreal y = 8.5 + i * 3.4;
        p.drawLine(QPointF(4.5, y), QPointF(i % 2 == 0 ? 8.5 : 7, y));
    }
    p.drawLine(QPointF(10, 20.5), QPointF(10, 18.2));
    p.drawLine(QPointF(14, 20.5), QPointF(14, 17.2));
}

void drawGallery(QPainter &p)
{
    QPainterPath frame;
    frame.addRoundedRect(QRectF(3, 4.5, 18, 15), 3, 3);
    p.drawPath(frame);
    QPainterPath hills;
    hills.moveTo(3.5, 16.5);
    hills.lineTo(8.5, 11.5);
    hills.lineTo(12.8, 15.5);
    hills.lineTo(15.8, 12.8);
    hills.lineTo(20.5, 17);
    p.drawPath(hills);
    p.drawEllipse(QPointF(16.2, 8.6), 1.5, 1.5);
}

void drawFullscreen(QPainter &p)
{
    const qreal a = 4, b = 9.5, c = 14.5, d = 20;
    p.drawPolyline(QPolygonF({QPointF(a, b), QPointF(a, a), QPointF(b, a)}));
    p.drawPolyline(QPolygonF({QPointF(c, a), QPointF(d, a), QPointF(d, b)}));
    p.drawPolyline(QPolygonF({QPointF(d, c), QPointF(d, d), QPointF(c, d)}));
    p.drawPolyline(QPolygonF({QPointF(b, d), QPointF(a, d), QPointF(a, c)}));
}

void drawMicroscope(QPainter &p)
{
    p.save();
    p.translate(11, 9);
    p.rotate(-22);
    QPainterPath tube;
    tube.addRoundedRect(QRectF(-2.4, -7, 4.8, 12), 1.6, 1.6);
    p.drawPath(tube);
    p.drawLine(QPointF(-1.6, 7.2), QPointF(1.6, 7.2));
    p.restore();
    QPainterPath arm;
    arm.moveTo(15.2, 10.8);
    arm.cubicTo(20, 12, 19.6, 18.4, 14, 19.4);
    p.drawPath(arm);
    p.drawLine(QPointF(5, 20.5), QPointF(19, 20.5));
    p.drawLine(QPointF(7.6, 16.3), QPointF(12.6, 16.3));
}

void drawBook(QPainter &p)
{
    QPainterPath book;
    book.moveTo(12, 6);
    book.cubicTo(9.5, 4.2, 6.2, 4, 3.5, 5);
    book.lineTo(3.5, 18.8);
    book.cubicTo(6.2, 17.9, 9.5, 18.2, 12, 20);
    book.cubicTo(14.5, 18.2, 17.8, 17.9, 20.5, 18.8);
    book.lineTo(20.5, 5);
    book.cubicTo(17.8, 4, 14.5, 4.2, 12, 6);
    p.drawPath(book);
    p.drawLine(QPointF(12, 6), QPointF(12, 20));
}

} // namespace

QPixmap lineIconPixmap(LineIcon icon, const QColor &color, int logicalSize, qreal dpr)
{
    const int pixels = qMax(1, qRound(logicalSize * dpr));
    QPixmap pixmap(pixels, pixels);
    pixmap.setDevicePixelRatio(dpr);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.scale(static_cast<qreal>(logicalSize) / 24.0, static_cast<qreal>(logicalSize) / 24.0);

    QPen pen(color, 1.7);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    switch (icon) {
    case LineIcon::Camera: drawCamera(painter); break;
    case LineIcon::Video: drawVideo(painter); break;
    case LineIcon::Stop: drawStop(painter); break;
    case LineIcon::Sparkle: drawSparkle(painter); break;
    case LineIcon::Analysis: drawAnalysis(painter); break;
    case LineIcon::Gallery: drawGallery(painter); break;
    case LineIcon::Fullscreen: drawFullscreen(painter); break;
    case LineIcon::Microscope: drawMicroscope(painter); break;
    case LineIcon::Book: drawBook(painter); break;
    }
    return pixmap;
}
