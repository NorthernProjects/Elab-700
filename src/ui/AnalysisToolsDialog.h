#pragma once

#include <QDialog>
#include <QImage>
#include <QPair>
#include <QVector>
#include <QWidget>

class QLabel;
class QPushButton;
class QSpinBox;
class AppSettings;

// Lab edition analysis canvas: click to place numbered counting markers,
// drag to measure distances (converted to µm via the scale-bar calibration),
// optional counting grid. All annotation coordinates are stored in NATIVE
// image pixels so the saved annotated copy is resolution-exact, and so the
// µm conversion uses the same "per native pixel" calibration the scale bar
// uses (digital zoom only crops, never resamples — see VideoView).
class AnalysisCanvas : public QWidget {
    Q_OBJECT

public:
    enum class Mode { Count, Measure };

    explicit AnalysisCanvas(QWidget *parent = nullptr);

    void setImage(const QImage &image);
    bool hasImage() const { return !m_image.isNull(); }

    void setMode(Mode mode) { m_mode = mode; }
    void setShowGrid(bool show);
    void setGridCellPx(int cellPx);
    void setMicronsPer100Px(double value);

    int markerCount() const { return m_markers.size(); }
    void undoLast();
    void clearAll();

    // Annotations burned into a full-resolution copy of the image.
    QImage renderAnnotated() const;

signals:
    void annotationsChanged();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    enum class ActionKind { Marker, Measure };

    QRect imageDisplayRect() const;
    QPoint widgetToImage(const QPoint &widgetPoint) const;
    QString measureText(const QPoint &a, const QPoint &b) const;
    void drawAnnotations(QPainter &painter, double scale, bool forExport) const;

    QImage m_image;
    Mode m_mode = Mode::Count;
    bool m_showGrid = false;
    int m_gridCellPx = 100;
    double m_micronsPer100Px = 0.0;

    QVector<QPoint> m_markers;                    // image coords
    QVector<QPair<QPoint, QPoint>> m_measures;    // image coords
    QVector<ActionKind> m_history;                // for true-order undo

    bool m_dragging = false;
    QPoint m_dragStart;
    QPoint m_dragCurrent;
};

// "Analyse" dialog (lab edition): hosts the canvas plus mode/grid controls,
// live marker count, open-any-image, and save-annotated-copy.
class AnalysisToolsDialog : public QDialog {
    Q_OBJECT

public:
    AnalysisToolsDialog(AppSettings *settings, const QImage &snapshot, QWidget *parent = nullptr);

private slots:
    void onOpenImage();
    void onSaveAnnotated();
    void onAnnotationsChanged();

private:
    AppSettings *m_settings;
    AnalysisCanvas *m_canvas;
    QLabel *m_countLabel;
    QPushButton *m_countModeButton;
    QPushButton *m_measureModeButton;
    QSpinBox *m_gridCellSpin;
};
