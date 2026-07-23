#pragma once

#include <QSize>
#include <QWidget>

class QLabel;
class QPushButton;

// Small card shown next to the video image when the user clicks "OMAX 83S"
// in the top bar. Static microscope characteristics plus the camera's live
// resolution/fps (fed by MainWindow, same values shown in the top bar) so
// nothing here can drift out of sync with what's actually connected.
class MicroscopeInfoPanel : public QWidget {
    Q_OBJECT

public:
    explicit MicroscopeInfoPanel(QWidget *parent = nullptr);

public slots:
    void setResolution(const QSize &size);
    void setFps(double fps);
    void setConnected(bool connected);

    // Card title = the user's own microscope name from the settings
    // ("Microscope" while unset).
    void setMicroscopeName(const QString &name);

    // Diagram + glossary buttons, driven by the learning-aids feature flag.
    void setLearningAidsVisible(bool visible);

signals:
    void closeRequested();

private:
    QLabel *m_titleLabel;
    QLabel *m_resolutionValue;
    QLabel *m_fpsValue;
    QLabel *m_connectionValue;
    QPushButton *m_diagramButton;
    QPushButton *m_glossaryButton;
};
