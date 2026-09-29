#include "PhotoEditorDialog.h"

#include <cmath>

#include <QButtonGroup>
#include <QColorDialog>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QTransform>
#include <QVBoxLayout>

namespace {
constexpr int kMaxUndoDepth = 20;

// Shared by the live preview (widget space) and the final bake (image
// space) so the two never drift apart.
void drawArrow(QPainter &painter, const QPoint &from, const QPoint &to, const QColor &color, int penWidth)
{
    QPen pen(color);
    pen.setWidth(penWidth);
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);
    painter.setBrush(color);
    painter.drawLine(from, to);

    const QLineF line(from, to);
    if (line.length() < 1.0)
        return;
    const double angle = std::atan2(line.dy(), line.dx());
    constexpr double kHeadLength = 18.0;
    constexpr double kHeadAngle = 0.5; // radians, ~29°
    const QPointF p1 = to - QPointF(std::cos(angle - kHeadAngle), std::sin(angle - kHeadAngle)) * kHeadLength;
    const QPointF p2 = to - QPointF(std::cos(angle + kHeadAngle), std::sin(angle + kHeadAngle)) * kHeadLength;
    const QPolygonF head({to, p1, p2});
    painter.drawPolygon(head);
}

void drawCircle(QPainter &painter, const QRect &boundingRect, const QColor &color, int penWidth)
{
    QPen pen(color);
    pen.setWidth(penWidth);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(boundingRect);
}
}

PhotoCanvas::PhotoCanvas(QWidget *parent) : QWidget(parent)
{
    setMinimumSize(320, 240);
    setMouseTracking(false);
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor("#05070a"));
    setPalette(pal);
}

void PhotoCanvas::setImage(const QImage &image)
{
    m_image = image.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    m_undoStack.clear();
    m_cropRect = QRect();
    update();
}

void PhotoCanvas::setTool(Tool tool)
{
    m_tool = tool;
    m_cropRect = QRect();
    m_shapeDragging = false;
    update();
}

void PhotoCanvas::setPenColor(const QColor &color)
{
    m_penColor = color;
}

bool PhotoCanvas::hasPendingCrop() const
{
    return m_tool == Tool::Crop && m_cropRect.isValid() && m_cropRect.width() > 4 && m_cropRect.height() > 4;
}

void PhotoCanvas::pushUndoSnapshot()
{
    if (m_undoStack.size() >= kMaxUndoDepth)
        m_undoStack.removeFirst();
    m_undoStack.append(m_image);
}

void PhotoCanvas::undo()
{
    if (m_undoStack.isEmpty())
        return;
    m_image = m_undoStack.takeLast();
    m_cropRect = QRect();
    update();
    emit imageChanged();
}

void PhotoCanvas::rotateLeft()
{
    if (m_image.isNull())
        return;
    pushUndoSnapshot();
    QTransform t;
    t.rotate(-90);
    m_image = m_image.transformed(t, Qt::SmoothTransformation);
    update();
    emit imageChanged();
}

void PhotoCanvas::rotateRight()
{
    if (m_image.isNull())
        return;
    pushUndoSnapshot();
    QTransform t;
    t.rotate(90);
    m_image = m_image.transformed(t, Qt::SmoothTransformation);
    update();
    emit imageChanged();
}

void PhotoCanvas::applyCrop()
{
    if (!hasPendingCrop())
        return;
    pushUndoSnapshot();
    m_image = m_image.copy(m_cropRect.intersected(m_image.rect()));
    m_cropRect = QRect();
    update();
    emit imageChanged();
}

QRect PhotoCanvas::imageDisplayRect() const
{
    if (m_image.isNull())
        return rect();
    const QSize target = m_image.size().scaled(size(), Qt::KeepAspectRatio);
    return QRect(QPoint((width() - target.width()) / 2, (height() - target.height()) / 2), target);
}

QPoint PhotoCanvas::widgetToImage(const QPoint &widgetPoint) const
{
    const QRect r = imageDisplayRect();
    if (m_image.isNull() || r.width() <= 0 || r.height() <= 0)
        return QPoint(-1, -1);
    const double sx = static_cast<double>(m_image.width()) / r.width();
    const double sy = static_cast<double>(m_image.height()) / r.height();
    const int x = qBound(0, static_cast<int>((widgetPoint.x() - r.left()) * sx), m_image.width() - 1);
    const int y = qBound(0, static_cast<int>((widgetPoint.y() - r.top()) * sy), m_image.height() - 1);
    return QPoint(x, y);
}

QPoint PhotoCanvas::imageToWidget(const QPoint &imagePoint) const
{
    const QRect r = imageDisplayRect();
    if (m_image.isNull() || m_image.width() <= 0 || m_image.height() <= 0)
        return imagePoint;
    const double sx = static_cast<double>(r.width()) / m_image.width();
    const double sy = static_cast<double>(r.height()) / m_image.height();
    return QPoint(r.left() + static_cast<int>(imagePoint.x() * sx), r.top() + static_cast<int>(imagePoint.y() * sy));
}

void PhotoCanvas::paintEvent(QPaintEvent * /*event*/)
{
    QPainter painter(this);
    painter.fillRect(rect(), QColor("#05070a"));

    if (m_image.isNull())
        return;

    const QRect displayRect = imageDisplayRect();
    painter.drawImage(displayRect, m_image);

    if (m_shapeDragging && (m_tool == Tool::Arrow || m_tool == Tool::Circle)) {
        painter.setRenderHint(QPainter::Antialiasing);
        const QPoint from = imageToWidget(m_shapeStartImagePoint);
        const QPoint to = imageToWidget(m_shapeCurrentImagePoint);
        if (m_tool == Tool::Arrow)
            drawArrow(painter, from, to, m_penColor, 3);
        else
            drawCircle(painter, QRect(from, to).normalized(), m_penColor, 3);
    }

    if (m_tool == Tool::Crop && m_cropRect.isValid()) {
        const double sx = static_cast<double>(displayRect.width()) / m_image.width();
        const double sy = static_cast<double>(displayRect.height()) / m_image.height();
        const QRect widgetCropRect(
            displayRect.left() + static_cast<int>(m_cropRect.left() * sx),
            displayRect.top() + static_cast<int>(m_cropRect.top() * sy),
            static_cast<int>(m_cropRect.width() * sx),
            static_cast<int>(m_cropRect.height() * sy));

        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0, 0, 0, 120));
        QRegion dim(displayRect);
        dim -= QRegion(widgetCropRect);
        painter.setClipRegion(dim);
        painter.drawRect(displayRect);
        painter.setClipping(false);

        QPen pen(Qt::white);
        pen.setStyle(Qt::DashLine);
        pen.setWidth(2);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        painter.drawRect(widgetCropRect);
    }
}

void PhotoCanvas::mousePressEvent(QMouseEvent *event)
{
    if (m_image.isNull() || event->button() != Qt::LeftButton)
        return;

    const QPoint imagePoint = widgetToImage(event->pos());
    if (imagePoint.x() < 0)
        return;

    switch (m_tool) {
    case Tool::Pen:
        pushUndoSnapshot();
        m_stroking = true;
        m_lastImagePoint = imagePoint;
        break;
    case Tool::Text: {
        bool ok = false;
        const QString text = QInputDialog::getText(this, tr("Ajouter un texte"),
                                                     tr("Texte :"), QLineEdit::Normal, QString(), &ok);
        if (ok && !text.trimmed().isEmpty()) {
            pushUndoSnapshot();
            QPainter painter(&m_image);
            painter.setRenderHint(QPainter::Antialiasing);
            QFont font = painter.font();
            font.setPointSize(18);
            font.setBold(true);
            painter.setFont(font);
            painter.setPen(m_penColor);
            painter.drawText(imagePoint, text);
            update();
            emit imageChanged();
        }
        break;
    }
    case Tool::Arrow:
    case Tool::Circle:
        m_shapeDragging = true;
        m_shapeStartImagePoint = imagePoint;
        m_shapeCurrentImagePoint = imagePoint;
        break;
    case Tool::Crop:
        m_cropDragging = true;
        m_cropStartImagePoint = imagePoint;
        m_cropRect = QRect(imagePoint, QSize(0, 0));
        break;
    }
    update();
}

void PhotoCanvas::mouseMoveEvent(QMouseEvent *event)
{
    if (m_image.isNull())
        return;
    const QPoint imagePoint = widgetToImage(event->pos());
    if (imagePoint.x() < 0)
        return;

    if (m_tool == Tool::Pen && m_stroking) {
        QPainter painter(&m_image);
        painter.setRenderHint(QPainter::Antialiasing);
        QPen pen(m_penColor);
        pen.setWidth(4);
        pen.setCapStyle(Qt::RoundCap);
        painter.setPen(pen);
        painter.drawLine(m_lastImagePoint, imagePoint);
        m_lastImagePoint = imagePoint;
        update();
    } else if ((m_tool == Tool::Arrow || m_tool == Tool::Circle) && m_shapeDragging) {
        m_shapeCurrentImagePoint = imagePoint;
        update();
    } else if (m_tool == Tool::Crop && m_cropDragging) {
        m_cropRect = QRect(m_cropStartImagePoint, imagePoint).normalized();
        update();
    }
}

void PhotoCanvas::mouseReleaseEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    if (m_tool == Tool::Pen && m_stroking) {
        m_stroking = false;
        emit imageChanged();
    }
    if ((m_tool == Tool::Arrow || m_tool == Tool::Circle) && m_shapeDragging) {
        m_shapeDragging = false;
        // A tap with no real drag draws nothing worth keeping — skip the
        // undo snapshot and bake entirely rather than leaving a stray dot.
        if (m_shapeStartImagePoint != m_shapeCurrentImagePoint) {
            pushUndoSnapshot();
            QPainter painter(&m_image);
            painter.setRenderHint(QPainter::Antialiasing);
            if (m_tool == Tool::Arrow)
                drawArrow(painter, m_shapeStartImagePoint, m_shapeCurrentImagePoint, m_penColor, 4);
            else
                drawCircle(painter, QRect(m_shapeStartImagePoint, m_shapeCurrentImagePoint).normalized(), m_penColor, 4);
            emit imageChanged();
        }
        update();
    }
    m_cropDragging = false;
}

PhotoEditorDialog::PhotoEditorDialog(const QString &imagePath, QWidget *parent)
    : QDialog(parent), m_imagePath(imagePath)
{
    setWindowTitle(tr("Modifier la photo"));
    resize(820, 640);

    auto *root = new QVBoxLayout(this);

    m_canvas = new PhotoCanvas(this);
    m_canvas->setImage(QImage(imagePath));
    root->addWidget(m_canvas, 1);

    auto *toolRow = new QHBoxLayout();
    auto *penButton = new QPushButton(tr("✏ Dessiner"), this);
    auto *textButton = new QPushButton(tr("🔤 Texte"), this);
    auto *arrowButton = new QPushButton(tr("➜ Flèche"), this);
    auto *circleButton = new QPushButton(tr("◯ Cercle"), this);
    auto *cropButton = new QPushButton(tr("⬛ Rogner"), this);
    penButton->setCheckable(true);
    textButton->setCheckable(true);
    arrowButton->setCheckable(true);
    circleButton->setCheckable(true);
    cropButton->setCheckable(true);
    penButton->setChecked(true);

    auto *toolGroup = new QButtonGroup(this);
    toolGroup->addButton(penButton);
    toolGroup->addButton(textButton);
    toolGroup->addButton(arrowButton);
    toolGroup->addButton(circleButton);
    toolGroup->addButton(cropButton);
    toolGroup->setExclusive(true);

    auto *colorButton = new QPushButton(tr("Couleur"), this);
    auto *rotateLeftButton = new QPushButton(QStringLiteral("↺"), this);
    auto *rotateRightButton = new QPushButton(QStringLiteral("↻"), this);
    auto *applyCropButton = new QPushButton(tr("Appliquer le rognage"), this);
    auto *undoButton = new QPushButton(tr("Annuler"), this);

    toolRow->addWidget(penButton);
    toolRow->addWidget(textButton);
    toolRow->addWidget(arrowButton);
    toolRow->addWidget(circleButton);
    toolRow->addWidget(colorButton);
    toolRow->addWidget(cropButton);
    toolRow->addWidget(applyCropButton);
    toolRow->addWidget(rotateLeftButton);
    toolRow->addWidget(rotateRightButton);
    toolRow->addWidget(undoButton);
    toolRow->addStretch();
    root->addLayout(toolRow);

    auto *bottomRow = new QHBoxLayout();
    auto *saveButton = new QPushButton(tr("Enregistrer"), this);
    auto *closeButton = new QPushButton(tr("Fermer"), this);
    bottomRow->addStretch();
    bottomRow->addWidget(saveButton);
    bottomRow->addWidget(closeButton);
    root->addLayout(bottomRow);

    connect(penButton, &QPushButton::clicked, this, [this] { m_canvas->setTool(PhotoCanvas::Tool::Pen); });
    connect(textButton, &QPushButton::clicked, this, [this] { m_canvas->setTool(PhotoCanvas::Tool::Text); });
    connect(arrowButton, &QPushButton::clicked, this, [this] { m_canvas->setTool(PhotoCanvas::Tool::Arrow); });
    connect(circleButton, &QPushButton::clicked, this, [this] { m_canvas->setTool(PhotoCanvas::Tool::Circle); });
    connect(cropButton, &QPushButton::clicked, this, [this] { m_canvas->setTool(PhotoCanvas::Tool::Crop); });
    connect(colorButton, &QPushButton::clicked, this, &PhotoEditorDialog::onPickColor);
    connect(rotateLeftButton, &QPushButton::clicked, m_canvas, &PhotoCanvas::rotateLeft);
    connect(rotateRightButton, &QPushButton::clicked, m_canvas, &PhotoCanvas::rotateRight);
    connect(applyCropButton, &QPushButton::clicked, m_canvas, &PhotoCanvas::applyCrop);
    connect(undoButton, &QPushButton::clicked, m_canvas, &PhotoCanvas::undo);
    connect(saveButton, &QPushButton::clicked, this, &PhotoEditorDialog::onSave);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
}

void PhotoEditorDialog::onPickColor()
{
    const QColor color = QColorDialog::getColor(Qt::red, this, tr("Couleur d'annotation"));
    if (color.isValid())
        m_canvas->setPenColor(color);
}

void PhotoEditorDialog::onSave()
{
    if (!m_canvas->image().save(m_imagePath)) {
        QMessageBox::warning(this, tr("Enregistrer"), tr("Impossible d'enregistrer la photo."));
        return;
    }
    QMessageBox::information(this, tr("Enregistrer"), tr("Photo enregistrée."));
}
