#pragma once

#include <QDialog>
#include <QImage>
#include <QVector>
#include <QWidget>

#include "core/AppSettings.h"

class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;

// Click surface for the calibration wizard: shows a still snapshot of the
// current frame, lets the user click two points (e.g. the two ends of a
// ruler placed under the microscope), and reports the distance between
// them in the frame's own native pixels.
class CalibrationImageWidget : public QWidget {
    Q_OBJECT

public:
    explicit CalibrationImageWidget(QWidget *parent = nullptr);

    void setImage(const QImage &image);
    void resetPoints();
    bool hasTwoPoints() const { return m_points.size() == 2; }
    double pixelDistance() const;

signals:
    void pointsChanged();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private:
    QRect imageDisplayRect() const;
    QPoint widgetToImage(const QPoint &widgetPoint) const;

    QImage m_image;
    QVector<QPoint> m_points;
};

// Guided scale-bar calibration: click two points of known real-world
// distance on the current frame (a ruler, a stage micrometer, anything
// measurable) and enter that distance — replaces guessing at a raw
// "µm per 100px" number by hand.
class ScaleCalibrationDialog : public QDialog {
    Q_OBJECT

public:
    ScaleCalibrationDialog(const QImage &snapshot, AppSettings *settings, QWidget *parent = nullptr);

private slots:
    void onPointsChanged();
    void onReset();
    void onConfirm();

private:
    AppSettings *m_settings;
    CalibrationImageWidget *m_canvas;
    QLabel *m_instructionLabel;
    QDoubleSpinBox *m_realDistanceSpin;
    QComboBox *m_unitCombo;
    QPushButton *m_confirmButton;
};
