#pragma once

#include <QScrollArea>

class QPropertyAnimation;

// QScrollArea's default mouse-wheel scrolling jumps the content in sudden,
// jerky steps. This animates each wheel step instead, so scrolling through
// (e.g.) the teacher panel actually feels smooth.
class SmoothScrollArea : public QScrollArea {
    Q_OBJECT

public:
    using QScrollArea::QScrollArea;

protected:
    void wheelEvent(QWheelEvent *event) override;

private:
    QPropertyAnimation *m_scrollAnimation = nullptr;
    int m_targetValue = 0;
};
