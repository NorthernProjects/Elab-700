#include "WindowChromeMac.h"

#import <AppKit/AppKit.h>

#include <QWidget>
#include <QWindow>

void applyMacTransparentTitlebar(QWidget *window)
{
    // winId() forces creation of the underlying native NSView/NSWindow if
    // it doesn't exist yet — safe to call before show(). WId is an integer
    // (quintptr), so under ARC the cast to an Objective-C object pointer
    // must go through __bridge — a plain reinterpret_cast<NSView *> is
    // rejected at compile time ("disallowed with ARC").
    NSView *view = (__bridge NSView *)reinterpret_cast<void *>(window->winId());
    NSWindow *nsWindow = view.window;
    if (!nsWindow)
        return;

    nsWindow.titlebarAppearsTransparent = YES;
    nsWindow.titleVisibility = NSWindowTitleHidden;
    nsWindow.styleMask |= NSWindowStyleMaskFullSizeContentView;
}
