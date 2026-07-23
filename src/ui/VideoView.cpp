#include "VideoView.h"

#include <QMouseEvent>
#include <QPainter>

VideoView::VideoView(QWidget *parent) : QWidget(parent)
{
    setMinimumSize(320, 240);
    setAutoFillBackground(true);

    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor("#05070a"));
    setPalette(pal);
}

void VideoView::setLightTheme(bool light)
{
    if (m_lightTheme == light)
        return;
    m_lightTheme = light;
    update();
}

void VideoView::setImmersive(bool immersive)
{
    if (m_immersive == immersive)
        return;
    m_immersive = immersive;
    update();
}

void VideoView::setShowGrid(bool show)
{
    if (m_showGrid == show)
        return;
    m_showGrid = show;
    update();
}

void VideoView::setFocusScore(double score0to100)
{
    m_focusScore = score0to100;
    if (m_showFocusIndicator)
        update();
}

void VideoView::setShowFocusIndicator(bool show)
{
    if (m_showFocusIndicator == show)
        return;
    m_showFocusIndicator = show;
    update();
}

void VideoView::setShowScaleBar(bool show)
{
    if (m_showScaleBar == show)
        return;
    m_showScaleBar = show;
    update();
}

void VideoView::setScaleBarCalibration(double micronsPer100Px)
{
    m_scaleBarMicronsPer100Px = micronsPer100Px;
    if (m_showScaleBar)
        update();
}

void VideoView::setLabTimerText(const QString &text)
{
    if (m_labTimerText == text)
        return;
    m_labTimerText = text;
    update();
}

void VideoView::setFrame(const QImage &image)
{
    m_currentFrame = image;
    update();
}

void VideoView::setCameraConnected(bool connected)
{
    if (m_cameraConnected == connected)
        return;
    m_cameraConnected = connected;
    if (!connected) {
        m_currentFrame = QImage();
        m_gridBadgeRect = QRect();
    }
    update();
}

void VideoView::paintEvent(QPaintEvent * /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    // Letterboxing bars around the camera image must match the active
    // theme (white in light mode) instead of always being black — a fixed
    // dark fill looked like two stray black bars once the rest of the UI
    // switched to light mode.
    painter.fillRect(rect(), m_lightTheme ? QColor("#f4f7f8") : QColor("#05070a"));

    if (!m_cameraConnected || m_currentFrame.isNull()) {
        painter.setPen(m_lightTheme ? QColor("#0e8a91") : QColor("#5ce1e6"));
        QFont font = painter.font();
        font.setPointSize(20);
        font.setBold(true);
        painter.setFont(font);
        painter.drawText(rect(), Qt::AlignCenter,
                          QStringLiteral("📷  Aucune caméra détectée\nBranchez la caméra du microscope"));
        return;
    }

    // Immersive ("plein écran ++") mode crops to fill the whole screen edge
    // to edge instead of letterboxing — that's the point of that mode, a
    // pure image with no bars. Normal mode keeps the full frame visible
    // (fit, not fill) so nothing captured is ever cropped off-view.
    const Qt::AspectRatioMode scaleMode = m_immersive ? Qt::KeepAspectRatioByExpanding : Qt::KeepAspectRatio;
    const QSize target = m_currentFrame.size().scaled(size(), scaleMode);
    const QRect destRect(QPoint((width() - target.width()) / 2, (height() - target.height()) / 2), target);
    if (m_immersive)
        painter.setClipRect(rect());
    painter.drawImage(destRect, m_currentFrame);

    // Plein écran ++ means just the image, nothing else — all overlays
    // (grid, scale bar, indicators) are suppressed there, consistent with
    // why that mode exists in the first place.
    if (!m_immersive) {
        if (m_showGrid)
            drawGrid(painter, destRect);
        if (m_showScaleBar && m_scaleBarMicronsPer100Px > 0.0)
            drawScaleBar(painter, destRect);
        if (m_showFocusIndicator)
            drawFocusIndicator(painter);
        // Always drawn (regardless of m_showGrid) — this badge is the
        // toggle control itself, so it has to stay visible/clickable even
        // while grid lines are off.
        drawGridBadge(painter);
    } else {
        m_gridBadgeRect = QRect();
    }

    // Shown even in Plein écran ++: a class-wide countdown is information
    // students actually need, not a UI control to hide away like the
    // grid/focus badges.
    if (!m_labTimerText.isEmpty())
        drawLabTimer(painter);
}

void VideoView::drawGrid(QPainter &painter, const QRect &imageRect) const
{
    // Simple rule-of-thirds framing aid, not tied to the image content.
    QPen pen(QColor(255, 255, 255, 130));
    pen.setWidth(1);
    painter.setPen(pen);
    for (int i = 1; i <= 2; ++i) {
        const int x = imageRect.left() + imageRect.width() * i / 3;
        painter.drawLine(x, imageRect.top(), x, imageRect.bottom());
        const int y = imageRect.top() + imageRect.height() * i / 3;
        painter.drawLine(imageRect.left(), y, imageRect.right(), y);
    }
}

void VideoView::drawFocusIndicator(QPainter &painter) const
{
    // A rough sharpness indicator (see MainWindow's computeSharpnessScore),
    // not a scientific autofocus measurement — just a rough aid for
    // students turning the focus knob.
    const QColor color = m_focusScore >= 66.0 ? QColor("#35e08a")
                        : m_focusScore >= 33.0 ? QColor("#e0c235")
                                                : QColor("#ff5c6c");
    const QRect badgeRect(16, 16, 160, 28);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 140));
    painter.drawRoundedRect(badgeRect, 6, 6);

    const QRect barRect = badgeRect.adjusted(10, 9, -60, -9);
    painter.setBrush(QColor(255, 255, 255, 60));
    painter.drawRect(barRect);
    QRect fillRect = barRect;
    fillRect.setWidth(static_cast<int>(barRect.width() * qBound(0.0, m_focusScore / 100.0, 1.0)));
    painter.setBrush(color);
    painter.drawRect(fillRect);

    painter.setPen(color);
    QFont font = painter.font();
    font.setPointSize(9);
    font.setBold(true);
    painter.setFont(font);
    painter.drawText(badgeRect.adjusted(0, 0, -8, 0), Qt::AlignRight | Qt::AlignVCenter,
                      QStringLiteral("Netteté"));
}

void VideoView::drawGridBadge(QPainter &painter)
{
    // Stacked directly below the "Netteté" badge (or in its spot if that
    // one is hidden) so the two view-control indicators read as one related
    // group — green when grid lines are on, red when off, same
    // traffic-light language as the focus indicator's colors.
    const QColor color = m_showGrid ? QColor("#35e08a") : QColor("#ff5c6c");
    const int y = m_showFocusIndicator ? 16 + 28 + 8 : 16;
    const QRect badgeRect(16, y, 110, 28);

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 140));
    painter.drawRoundedRect(badgeRect, 6, 6);

    painter.setPen(color);
    QFont font = painter.font();
    font.setPointSize(9);
    font.setBold(true);
    painter.setFont(font);
    painter.drawText(badgeRect, Qt::AlignCenter, QStringLiteral("● Grille"));

    m_gridBadgeRect = badgeRect;
}

void VideoView::drawLabTimer(QPainter &painter) const
{
    // Top-right corner — mirrors the grid/netteté badges on the left, but
    // visually distinct (amber) since this is class-wide timing info, not a
    // per-student view control.
    QFont font = painter.font();
    font.setPointSize(16);
    font.setBold(true);
    painter.setFont(font);
    const QFontMetrics metrics(font);
    const int textWidth = metrics.horizontalAdvance(m_labTimerText);
    const QRect badgeRect(width() - textWidth - 36, 16, textWidth + 20, 34);

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 150));
    painter.drawRoundedRect(badgeRect, 6, 6);

    painter.setPen(QColor("#e0a835"));
    painter.drawText(badgeRect, Qt::AlignCenter, m_labTimerText);
}

void VideoView::drawScaleBar(QPainter &painter, const QRect &imageRect) const
{
    if (m_currentFrame.width() <= 0)
        return;

    // 1 pixel of m_currentFrame always equals 1 pixel of the camera's native
    // captured resolution — digital zoom only crops it (see MainWindow's
    // applyZoom, a plain QImage::copy with no resampling), it never
    // resamples. So the calibration (µm per 100 native pixels) combined with
    // this frame's on-screen scale factor is all that's needed here,
    // independent of zoom or window size.
    const double screenPxPerFramePx = static_cast<double>(imageRect.width()) / m_currentFrame.width();
    const double micronsPerFramePx = m_scaleBarMicronsPer100Px / 100.0;

    static const double kNiceMicronSteps[] = {1, 2, 5, 10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000};
    const double maxBarScreenPx = imageRect.width() * 0.3;

    double chosenMicrons = kNiceMicronSteps[0];
    for (double step : kNiceMicronSteps) {
        const double screenPx = (step / micronsPerFramePx) * screenPxPerFramePx;
        if (screenPx > maxBarScreenPx)
            break;
        chosenMicrons = step;
    }

    const double barScreenPx = (chosenMicrons / micronsPerFramePx) * screenPxPerFramePx;
    if (barScreenPx < 4.0 || barScreenPx > imageRect.width())
        return;

    const QString label = chosenMicrons >= 1000.0
        ? QStringLiteral("%1 mm").arg(chosenMicrons / 1000.0, 0, 'g', 3)
        : QStringLiteral("%1 µm").arg(chosenMicrons, 0, 'g', 3);

    const int barBottom = imageRect.bottom() - 20;
    const int barLeft = imageRect.left() + 20;
    const int barRight = barLeft + static_cast<int>(barScreenPx);

    QPen pen(Qt::white);
    pen.setWidth(3);
    painter.setPen(pen);
    painter.drawLine(barLeft, barBottom, barRight, barBottom);
    painter.drawLine(barLeft, barBottom - 5, barLeft, barBottom + 5);
    painter.drawLine(barRight, barBottom - 5, barRight, barBottom + 5);

    QFont font = painter.font();
    font.setPointSize(10);
    font.setBold(true);
    painter.setFont(font);
    painter.drawText(QRect(barLeft, barBottom - 26, static_cast<int>(barScreenPx) + 40, 20),
                      Qt::AlignLeft | Qt::AlignVCenter, label);
}

void VideoView::mouseDoubleClickEvent(QMouseEvent *event)
{
    QWidget::mouseDoubleClickEvent(event);
    emit doubleClicked();
}

void VideoView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && m_gridBadgeRect.contains(event->pos())) {
        emit gridToggleClicked();
        return;
    }
    QWidget::mousePressEvent(event);
}
