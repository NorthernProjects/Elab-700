#pragma once

#include <QColor>
#include <QPixmap>

// Thin, rounded line icons drawn in code (in the spirit of Apple's SF
// Symbols): one stroke color that follows the theme, crisp at any scale,
// identical on every Windows version — unlike emoji, whose look depends on
// the installed font.
enum class LineIcon {
    Camera,
    Video,
    Stop,
    Sparkle,
    Analysis,
    Gallery,
    Fullscreen,
    Microscope,
    Book
};

// logicalSize is in device-independent pixels; the pixmap is rendered at
// logicalSize * dpr so it stays sharp on high-DPI screens.
QPixmap lineIconPixmap(LineIcon icon, const QColor &color, int logicalSize, qreal dpr = 1.0);
