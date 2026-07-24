#pragma once

class QWidget;

// Makes the native macOS titlebar transparent and hides its text, while
// keeping the red/orange/green traffic-light buttons — our own top bar
// already shows the app name/logo, so the separate white titlebar strip
// with "E-Lab 700" repeated above it is redundant. Only built/linked on
// APPLE (see CMakeLists.txt) — call sites must guard with #ifdef
// Q_OS_MAC, this header isn't usable on other platforms.
void applyMacTransparentTitlebar(QWidget *window);
