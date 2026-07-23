#pragma once

#include <QImage>
#include <QWidget>

// Displays the live camera frame scaled to fit, preserving aspect ratio.
// When no frame has arrived (camera disconnected / SDK missing), shows a
// clear, unambiguous "no camera detected" message instead of any image —
// the app must never fabricate a video feed.
class VideoView : public QWidget {
    Q_OBJECT

public:
    explicit VideoView(QWidget *parent = nullptr);

    // The exact frame currently on screen — used by the scale bar
    // calibration wizard so what the teacher clicks on matches what they
    // were just looking at.
    QImage currentFrame() const { return m_currentFrame; }

public slots:
    void setFrame(const QImage &image);
    void setCameraConnected(bool connected);
    void setLightTheme(bool light);
    void setImmersive(bool immersive);
    void setShowGrid(bool show);
    void setFocusScore(double score0to100);
    void setShowFocusIndicator(bool show);
    void setShowScaleBar(bool show);
    void setScaleBarCalibration(double micronsPer100Px);
    void setLabTimerText(const QString &text);

signals:
    void doubleClicked();
    void gridToggleClicked();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    void drawGrid(QPainter &painter, const QRect &imageRect) const;
    void drawFocusIndicator(QPainter &painter) const;
    void drawGridBadge(QPainter &painter);
    void drawScaleBar(QPainter &painter, const QRect &imageRect) const;
    void drawLabTimer(QPainter &painter) const;

    QImage m_currentFrame;
    bool m_cameraConnected = false;
    bool m_lightTheme = false;
    bool m_immersive = false;
    bool m_showGrid = false;
    bool m_showFocusIndicator = false;
    double m_focusScore = 0.0;
    bool m_showScaleBar = false;
    double m_scaleBarMicronsPer100Px = 0.0;
    QRect m_gridBadgeRect;
    QString m_labTimerText;
};
