#pragma once

#include <QElapsedTimer>
#include <QPixmap>
#include <QVector>
#include <QWidget>

class QLabel;
class QTimer;

// Startup intro: the school logo + "E-LAB 700 / MICROSCOPIE" visible from
// the very first frame, pulsing like a heartbeat for as long as the app
// takes to load, with a field of drifting/twinkling science-themed glyphs
// behind it (like stars scrolling past in space). Purely decorative —
// playIntro() blocks for whatever's left of a fixed total duration (using a
// local event loop, since QApplication::exec() hasn't started yet) then
// returns. Deliberately not wired to MainWindow's own readiness: an earlier
// version tried to synchronize the two and ended up hiding a modal dialog
// behind this always-on-top splash, freezing the app with no visible way
// out. Call startAnimating() right after show() so the particles/breathing
// begin immediately instead of sitting frozen while the caller goes on to
// build MainWindow (which can take a moment) before calling playIntro().
class SplashScreen : public QWidget {
    Q_OBJECT

public:
    explicit SplashScreen(QWidget *parent = nullptr);

    void startAnimating();
    void playIntro();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    struct Particle {
        QPointF pos;
        QPointF velocity;
        QString glyph;
        qreal phase;
    };

    void tick();

    QVector<Particle> m_particles;
    QElapsedTimer m_clock;
    QTimer *m_tickTimer = nullptr;
    QWidget *m_container;
    QLabel *m_logoLabel;
    QLabel *m_titleLabel;
    QLabel *m_subtitleLabel;
    QPixmap m_logoPixmap;
};
