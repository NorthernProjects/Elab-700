#pragma once

#include <QPixmap>
#include <QWidget>

// "Before/after" reveal slider: draws the before image, then the after
// image clipped to the left of a draggable vertical divider — dragging
// anywhere on the widget moves the divider under the cursor. A more visual
// complement to ComparisonDialog's existing side-by-side view, not a
// replacement for it.
class RevealCompareWidget : public QWidget {
    Q_OBJECT

public:
    explicit RevealCompareWidget(QWidget *parent = nullptr);

    void setImages(const QPixmap &before, const QPixmap &after);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;

private:
    QRect displayRect() const;
    void updateDividerFromX(int x);

    QPixmap m_before;
    QPixmap m_after;
    double m_dividerFraction = 0.5; // 0 = all "after", 1 = all "before"
};
