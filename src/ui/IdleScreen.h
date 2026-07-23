#pragma once

#include <QWidget>

class QLabel;
class QGraphicsOpacityEffect;
class QPropertyAnimation;

// Full-window overlay shown after a period of inactivity: just the school's
// logo, gently pulsing, on a black background. Any mouse/keyboard activity
// (caught by MainWindow's application-wide event filter) hides it again.
class IdleScreen : public QWidget {
    Q_OBJECT

public:
    explicit IdleScreen(QWidget *parent = nullptr);

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    QLabel *m_logoLabel;
    QGraphicsOpacityEffect *m_opacityEffect;
    QPropertyAnimation *m_pulseAnimation;
};
