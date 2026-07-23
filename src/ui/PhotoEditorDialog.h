#pragma once

#include <QDialog>
#include <QImage>
#include <QVector>
#include <QWidget>

// Lightweight photo editor reachable from the gallery: freehand pen
// annotation, click-to-place text labels, crop, and 90° rotation, with a
// single-level-deep undo stack (image snapshots). Not a general image
// editor — just enough for a student to circle/label something they
// observed or straighten/crop a capture before keeping it.
class PhotoCanvas : public QWidget {
    Q_OBJECT

public:
    enum class Tool { Pen, Text, Crop };

    explicit PhotoCanvas(QWidget *parent = nullptr);

    void setImage(const QImage &image);
    QImage image() const { return m_image; }

    void setTool(Tool tool);
    void setPenColor(const QColor &color);
    bool hasPendingCrop() const;

public slots:
    void rotateLeft();
    void rotateRight();
    void applyCrop();
    void undo();

signals:
    void imageChanged();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

private:
    QRect imageDisplayRect() const;
    QPoint widgetToImage(const QPoint &widgetPoint) const;
    void pushUndoSnapshot();

    QImage m_image;
    QVector<QImage> m_undoStack;
    Tool m_tool = Tool::Pen;
    QColor m_penColor = Qt::red;

    QPoint m_lastImagePoint;
    bool m_stroking = false;

    QPoint m_cropStartImagePoint;
    QRect m_cropRect;
    bool m_cropDragging = false;
};

// The dialog wrapper: canvas plus a small toolbar (tool selection, color,
// rotate, crop, undo, save/close).
class PhotoEditorDialog : public QDialog {
    Q_OBJECT

public:
    PhotoEditorDialog(const QString &imagePath, QWidget *parent = nullptr);

private slots:
    void onPickColor();
    void onSave();

private:
    QString m_imagePath;
    PhotoCanvas *m_canvas;
};
