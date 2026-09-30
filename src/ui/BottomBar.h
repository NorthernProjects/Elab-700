#pragma once

#include <QPushButton>
#include <QWidget>

// Bottom action bar for the student screen: Photo, Video, Auto, Galerie,
// Plein écran, plus a small discreet digital zoom +/- control off to one
// side (the camera's own field of view is fixed by the microscope's optics,
// see README > Zoom numérique — this only crops/zooms the captured frame).
class BottomBar : public QWidget {
    Q_OBJECT

public:
    explicit BottomBar(QWidget *parent = nullptr);

    void setRecording(bool recording);

public slots:
    void setZoomPercent(int percent);

    // Schéma/Glossaire buttons, driven by the learning-aids feature flag.
    void setLearningAidsVisible(bool visible);

signals:
    void photoRequested();
    void videoToggleRequested();
    void autoRequested();
    void galleryRequested();
    void fullscreenRequested();
    void zoomInRequested();
    void zoomOutRequested();
    void zoomResetRequested();
    // Opens the counting/measuring analysis tools (common to all editions).
    void analysisRequested();
    void diagramRequested();
    void glossaryRequested();

private:
    QPushButton *m_photoButton;
    QPushButton *m_videoButton;
    QPushButton *m_autoButton;
    QPushButton *m_galleryButton;
    QPushButton *m_fullscreenButton;
    QPushButton *m_zoomOutButton;
    QPushButton *m_zoomInButton;
    QPushButton *m_zoomLabel;
    QPushButton *m_diagramButton;
    QPushButton *m_glossaryButton;
};
