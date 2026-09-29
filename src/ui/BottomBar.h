#pragma once

#include <QPushButton>
#include <QWidget>

class QLabel;
class QSlider;

// Bottom action bar for the student screen: Photo, Video, Auto, Galerie,
// Plein écran, plus a small discreet digital zoom +/- control off to one
// side (the camera's own field of view is fixed by the microscope's optics,
// see README > Zoom numérique — this only crops/zooms the captured frame)
// and a brightness slider off to the other — capped by the teacher (see
// AppSettings::maxBrightnessPercent) so a class can't wash the image out.
class BottomBar : public QWidget {
    Q_OBJECT

public:
    explicit BottomBar(QWidget *parent = nullptr);

    void setRecording(bool recording);

public slots:
    void setZoomPercent(int percent);
    void setBrightnessPercent(int percent);

    // Caps the slider's range at maxPercent (teacher-configured, see
    // AppSettings::maxBrightnessPercent) — the software-enforced limit the
    // slider itself can never be dragged past.
    void setBrightnessLimit(int maxPercent);

signals:
    void photoRequested();
    void videoToggleRequested();
    void autoRequested();
    void galleryRequested();
    void fullscreenRequested();
    void zoomInRequested();
    void zoomOutRequested();
    void zoomResetRequested();
    void brightnessChanged(int percent);
    // Opens the counting/measuring analysis tools (common to all editions).
    void analysisRequested();

private:
    QPushButton *m_photoButton;
    QPushButton *m_videoButton;
    QPushButton *m_autoButton;
    QPushButton *m_galleryButton;
    QPushButton *m_fullscreenButton;
    QPushButton *m_zoomOutButton;
    QPushButton *m_zoomInButton;
    QPushButton *m_zoomLabel;
    QSlider *m_brightnessSlider;
    QLabel *m_brightnessValueLabel;
};
