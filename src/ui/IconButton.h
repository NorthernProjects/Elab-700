#pragma once

#include <QPushButton>

#include "LineIcons.h"

// A push button that draws one of the LineIcons next to/above its text,
// using the button's styled text color — so it follows the theme and the
// hover/pressed/recording colors from the stylesheet with no extra code.
// The frame itself is still painted by the style (QSS), exactly like a
// normal QPushButton; only the content is custom.
class IconButton : public QPushButton {
public:
    enum class Layout {
        IconAboveText, // the big action buttons
        IconBesideText // compact secondary buttons
    };

    IconButton(LineIcon icon, const QString &label, Layout layout, QWidget *parent = nullptr);

    void setLineIcon(LineIcon icon);

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    LineIcon m_icon;
    Layout m_layout;
};
