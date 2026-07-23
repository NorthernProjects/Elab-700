#include "SplashScreen.h"

#include <cmath>
#include <cstdlib>

#include <QEventLoop>
#include <QGuiApplication>
#include <QLabel>
#include <QPainter>
#include <QScreen>
#include <QTimer>
#include <QVBoxLayout>

namespace {
constexpr int kParticleCount = 45;
constexpr int kTickIntervalMs = 30;
constexpr int kTotalDurationMs = 3200;
constexpr int kLogoBaseWidth = 280;
constexpr qreal kBreathingPeriodMs = 2600.0;

qreal randomUnit()
{
    return static_cast<qreal>(std::rand()) / RAND_MAX;
}

// Slow, smooth breathing pulse (not a sharp double-thump) — grows and
// shrinks the logo gently while the app loads.
qreal breathingScale(qreal elapsedMs)
{
    return 1.0 + 0.05 * std::sin(2.0 * M_PI * elapsedMs / kBreathingPeriodMs);
}
}

SplashScreen::SplashScreen(QWidget *parent) : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor("#05070a"));
    setPalette(pal);

    // Covers the whole screen (not just a centered box) so it fully hides
    // MainWindow — which is bigger than any fixed splash size — while it
    // constructs and shows itself behind this splash (see main.cpp).
    if (QScreen *screen = QGuiApplication::primaryScreen())
        setGeometry(screen->geometry());
    else
        setFixedSize(1280, 800);

    // Each entry is a full QString (not QChar): most of these glyphs are
    // outside the Basic Multilingual Plane and need a UTF-16 surrogate pair,
    // so a single QChar can only ever hold half of one (that half then
    // renders as a "?" tofu glyph — picking by QChar was the earlier bug).
    static const QStringList kGlyphs = {
        QStringLiteral("\U0001F52C"), QStringLiteral("\U0001F9EC"), QStringLiteral("\U0001F9EA"),
        QStringLiteral("\U00002697"), QStringLiteral("\U0001F9A0"), QStringLiteral("\U00002728"),
        QStringLiteral("\U0001F9EB"),
    };
    m_particles.reserve(kParticleCount);
    for (int i = 0; i < kParticleCount; ++i) {
        Particle p;
        p.pos = QPointF(randomUnit() * width(), randomUnit() * height());
        p.velocity = QPointF((randomUnit() - 0.5) * 0.6, (randomUnit() - 0.5) * 0.6);
        p.glyph = kGlyphs.at(std::rand() % kGlyphs.size());
        p.phase = randomUnit() * 6.28318;
        m_particles.append(p);
    }

    m_container = new QWidget(this);
    auto *containerLayout = new QVBoxLayout(m_container);
    containerLayout->setAlignment(Qt::AlignCenter);

    m_logoLabel = new QLabel(m_container);
    m_logoLabel->setAlignment(Qt::AlignCenter);
    m_logoLabel->setFixedHeight(kLogoBaseWidth); // room to grow into on each beat, no layout jitter
    // The plain square icon (not the full wordmark logo.png), for two
    // reasons: (1) logo.png already has "E-LAB 700"/tagline text baked in on
    // some brandings, which duplicated the title/subtitle labels drawn right
    // below it; (2) it's authored near its native resolution at the size
    // this widget displays it at, whereas the wide wordmark image had to be
    // downscaled hard enough to blur its own small text. icon.png has none
    // of that text, so there's nothing to duplicate or blur here.
    const QPixmap logo(QStringLiteral(":/branding/icon.png"));
    if (!logo.isNull()) {
        m_logoPixmap = logo.scaledToWidth(kLogoBaseWidth, Qt::SmoothTransformation);
        m_logoLabel->setPixmap(m_logoPixmap);
    }
    containerLayout->addWidget(m_logoLabel);

    // Fixed vertical gap (not just a stylesheet margin) so the breathing
    // logo above never grows into the title text below it.
    containerLayout->addSpacing(18);

    m_titleLabel = new QLabel(QStringLiteral("E-LAB 700"), m_container);
    m_titleLabel->setAlignment(Qt::AlignCenter);
    QFont titleFont = m_titleLabel->font();
    titleFont.setPointSize(26);
    titleFont.setBold(true);
    titleFont.setLetterSpacing(QFont::AbsoluteSpacing, 6);
    m_titleLabel->setFont(titleFont);
    m_titleLabel->setStyleSheet(QStringLiteral("color: #5ce1e6;"));
    containerLayout->addWidget(m_titleLabel);

    m_subtitleLabel = new QLabel(QStringLiteral("MICROSCOPIE"), m_container);
    m_subtitleLabel->setAlignment(Qt::AlignCenter);
    QFont subtitleFont = m_subtitleLabel->font();
    subtitleFont.setPointSize(13);
    subtitleFont.setLetterSpacing(QFont::AbsoluteSpacing, 4);
    m_subtitleLabel->setFont(subtitleFont);
    m_subtitleLabel->setStyleSheet(QStringLiteral("color: #9fd6d9;"));
    containerLayout->addWidget(m_subtitleLabel);

    auto *outerLayout = new QVBoxLayout(this);
    outerLayout->addWidget(m_container, 0, Qt::AlignCenter);
}

void SplashScreen::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);

    QPainter painter(this);
    painter.fillRect(rect(), QColor("#05070a"));

    QFont font = painter.font();
    font.setFamilies({QStringLiteral("Segoe UI Emoji"), QStringLiteral("Segoe UI Symbol")});
    font.setPointSize(20);
    painter.setFont(font);

    // Keep glyphs from drifting across the logo/title so "MICROSCOPIE"
    // always stays legible instead of occasionally landing under a symbol.
    const QRect textSafeZone = m_container->geometry().adjusted(-40, -40, 40, 40);

    const qreal elapsed = m_clock.isValid() ? static_cast<qreal>(m_clock.elapsed()) : 0.0;
    for (const Particle &p : m_particles) {
        if (textSafeZone.contains(p.pos.toPoint()))
            continue;
        const qreal opacity = 0.2 + 0.5 * std::abs(std::sin(elapsed / 700.0 + p.phase));
        painter.setOpacity(opacity);
        painter.drawText(QPointF(p.pos.x(), p.pos.y()), p.glyph);
    }

    // Small rotating "loading" arc below the logo/title block.
    const QPoint spinnerCenter(m_container->geometry().center().x(), m_container->geometry().bottom() + 40);
    constexpr qreal spinnerRadius = 14.0;
    QPen pen(QColor("#5ce1e6"));
    pen.setWidth(3);
    pen.setCapStyle(Qt::RoundCap);
    painter.setOpacity(0.85);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    const QRectF spinnerRect(spinnerCenter.x() - spinnerRadius, spinnerCenter.y() - spinnerRadius,
                              spinnerRadius * 2, spinnerRadius * 2);
    const int startAngle = static_cast<int>(std::fmod(elapsed / 3.0, 360.0) * 16);
    painter.drawArc(spinnerRect, startAngle, 100 * 16);
}

void SplashScreen::tick()
{
    for (Particle &p : m_particles) {
        p.pos += p.velocity;
        if (p.pos.x() < -20)
            p.pos.setX(width() + 20);
        else if (p.pos.x() > width() + 20)
            p.pos.setX(-20);
        if (p.pos.y() < -20)
            p.pos.setY(height() + 20);
        else if (p.pos.y() > height() + 20)
            p.pos.setY(-20);
    }

    if (!m_logoPixmap.isNull()) {
        const qreal elapsed = m_clock.isValid() ? static_cast<qreal>(m_clock.elapsed()) : 0.0;
        const qreal scale = breathingScale(elapsed);
        m_logoLabel->setPixmap(m_logoPixmap.scaled(m_logoPixmap.size() * scale, Qt::KeepAspectRatio,
                                                     Qt::SmoothTransformation));
    }

    update();
}

void SplashScreen::startAnimating()
{
    // Called right after show(), before the caller goes on to construct
    // MainWindow (which can take a moment): starts the clock and the tick
    // timer immediately so particles/breathing move from the very first
    // frame instead of sitting frozen until playIntro() is eventually
    // called.
    if (m_tickTimer)
        return;
    m_clock.start();
    m_tickTimer = new QTimer(this);
    connect(m_tickTimer, &QTimer::timeout, this, &SplashScreen::tick);
    m_tickTimer->start(kTickIntervalMs);
}

void SplashScreen::playIntro()
{
    // Caller is expected to have already called show() + startAnimating()
    // before constructing whatever comes next (MainWindow): staying on
    // screen while that happens hides its construction/first-paint behind
    // this splash instead of a blank white flash. This method waits out
    // whatever remains of the fixed total duration (some of it may have
    // already elapsed during MainWindow's construction), then closes —
    // deliberately not synchronized to MainWindow's own state (see class
    // comment).
    if (!m_tickTimer)
        startAnimating();

    const qreal elapsed = static_cast<qreal>(m_clock.elapsed());
    const int remaining = static_cast<int>(kTotalDurationMs - elapsed);
    if (remaining > 0) {
        QEventLoop loop;
        QTimer::singleShot(remaining, &loop, &QEventLoop::quit);
        loop.exec();
    }

    m_tickTimer->stop();
    hide();
}
