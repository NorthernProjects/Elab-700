#pragma once

#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QSize>
#include <QWidget>

class QMouseEvent;

// Top strip: camera connection indicator, fps, resolution on the left; the
// software/microscope name centered (clicking the microscope name shows its
// specs); and, on the right, the "group login" button (shows school/group
// once connected) next to a small, deliberately unobtrusive button to enter
// teacher mode (V1 requirement: "no technical menu visible on the main
// screen").
class TopStatusBar : public QWidget {
    Q_OBJECT

public:
    explicit TopStatusBar(QWidget *parent = nullptr);

public slots:
    void setConnected(bool connected, const QString &modelName = QString());
    void setFps(double fps);
    void setResolution(const QSize &size);
    void setGroupInfo(const QString &className, const QString &groupName);

    // The centered title shows the user's own microscope name from the
    // settings ("E-Lab 700 · <name>", or just "E-Lab 700" while unset)
    // instead of a hardcoded model.
    void setMicroscopeName(const QString &name);

    // Driven by MainWindow from the runtime feature flags (classes/groups
    // on or off depending on the chosen edition).
    void setGroupButtonVisible(bool visible);
    void setTeacherButtonToolTip(const QString &tip);

    // Extra left margin, in pixels, added on top of the normal content
    // margin — used on macOS to keep the logo/connection indicator clear of
    // the traffic-light buttons when the native titlebar is hidden (see
    // MainWindow / WindowChromeMac). No-op call on other platforms.
    void setLeftInset(int pixels);

protected:
#if defined(Q_OS_MAC)
    // Only needed on macOS: hiding the native titlebar (see MainWindow /
    // WindowChromeMac) also removes the OS's own click-and-drag-to-move
    // behavior for that area, so this bar has to provide it itself for
    // clicks on its empty background (buttons still get their own clicks
    // first, same as a real titlebar). No-op on other platforms, which keep
    // their native titlebar and don't need this.
    void mousePressEvent(QMouseEvent *event) override;
#endif

signals:
    void teacherModeRequested();
    void groupSelectionRequested();
    void microscopeInfoRequested();
    void resolutionClicked();
    void connectionClicked();
    void helpRequested();

private:
    QLabel *m_logoLabel;
    QPushButton *m_connectionButton;
    QPushButton *m_microscopeLabel;
    QPushButton *m_groupButton;
    QLabel *m_fpsLabel;
    QPushButton *m_resolutionButton;
    QPushButton *m_teacherButton;
    QPushButton *m_helpButton;
    QGridLayout *m_layout;
    int m_baseLeftMargin = 16;
};
