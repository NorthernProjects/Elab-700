#include "WindowChromeMac.h"

#import <AppKit/AppKit.h>

#include <QWidget>
#include <QWindow>

void applyMacTransparentTitlebar(QWidget *window)
{
    // winId() forces creation of the underlying native NSView/NSWindow if
    // it doesn't exist yet — safe to call before show().
    NSView *view = reinterpret_cast<NSView *>(window->winId());
    NSWindow *nsWindow = view.window;
    if (!nsWindow)
        return;

    nsWindow.titlebarAppearsTransparent = YES;
    nsWindow.titleVisibility = NSWindowTitleHidden;
    nsWindow.styleMask |= NSWindowStyleMaskFullSizeContentView;
}
