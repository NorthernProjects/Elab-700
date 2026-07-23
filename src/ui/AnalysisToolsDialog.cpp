#include "AnalysisToolsDialog.h"

#include <algorithm>
#include <cmath>

#include <QButtonGroup>
#include <QCheckBox>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineF>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include "core/AppSettings.h"

namespace {
constexpr int kMinMeasureLengthPx = 3;
}

AnalysisCanvas::AnalysisCanvas(QWidget *parent) : QWidget(parent)
{
    setMinimumSize(420, 300);
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor("#05070a"));
    setPalette(pal);
    setMouseTracking(false);
}

void AnalysisCanvas::setImage(const QImage &image)
{
    m_image = image;
    clearAll();
}

void AnalysisCanvas::setShowGrid(bool show)
{
    m_showGrid = show;
    update();
}

void AnalysisCanvas::setGridCellPx(int cellPx)
{
    m_gridCellPx = qMax(10, cellPx);
    update();
}

void AnalysisCanvas::setMicronsPer100Px(double value)
{
    m_micronsPer100Px = value;
    update();
}

void AnalysisCanvas::undoLast()
{
    if (m_history.isEmpty())
        return;
    const ActionKind last = m_history.takeLast();
    if (last == ActionKind::Marker && !m_markers.isEmpty())
        m_markers.removeLast();
    else if (last == ActionKind::Measure && !m_measures.isEmpty())
        m_measures.removeLast();
    update();
    emit annotationsChanged();
}

void AnalysisCanvas::clearAll()
{
    m_markers.clear();
    m_measures.clear();
    m_history.clear();
    m_dragging = false;
    update();
    emit annotationsChanged();
}

QRect AnalysisCanvas::imageDisplayRect() const
{
    if (m_image.isNull())
        return rect();
    const QSize target = m_image.size().scaled(size(), Qt::KeepAspectRatio);
    return QRect(QPoint((width() - target.width()) / 2, (height() - target.height()) / 2), target);
}

QPoint AnalysisCanvas::widgetToImage(const QPoint &widgetPoint) const
{
    const QRect r = imageDisplayRect();
    if (m_image.isNull() || r.width() <= 0 || r.height() <= 0)
        return QPoint(-1, -1);
    if (!r.contains(widgetPoint))
        return QPoint(-1, -1);
    const double sx = static_cast<double>(m_image.width()) / r.width();
    const double sy = static_cast<double>(m_image.height()) / r.height();
    const int x = qBound(0, static_cast<int>((widgetPoint.x() - r.left()) * sx), m_image.width() - 1);
    const int y = qBound(0, static_cast<int>((widgetPoint.y() - r.top()) * sy), m_image.height() - 1);
    return QPoint(x, y);
}

QString AnalysisCanvas::measureText(const QPoint &a, const QPoint &b) const
{
    const double pixels = QLineF(a, b).length();
    if (m_micronsPer100Px <= 0.0)
        return QStringLiteral("%1 px").arg(pixels, 0, 'f', 0);

    const double microns = pixels * m_micronsPer100Px / 100.0;
    if (microns >= 1000.0)
        return QStringLiteral("%1 mm").arg(microns / 1000.0, 0, 'f', 2);
    return QStringLiteral("%1 µm").arg(microns, 0, 'f', 1);
}

// Shared by the on-screen paint (scale = display/native ratio) and the
// full-resolution export (scale = 1, forExport = true): one code path so
// what you see is exactly what gets saved.
void AnalysisCanvas::drawAnnotations(QPainter &painter, double scale, bool forExport) const
{
    const QColor accent("#5ce1e6");
    const QColor markerText("#06281f");
    const QColor measureColor("#ffd166");

    // Sizes in DISPLAY units on screen; for export they're derived from the
    // native image width so annotations stay readable at any resolution.
    const double markerRadius = forExport ? qMax(9.0, m_image.width() / 120.0) : 11.0;
    const double lineWidth = forExport ? qMax(2.0, m_image.width() / 640.0) : 2.0;
    const int fontSize = forExport ? qMax(12, m_image.width() / 60) : 11;

    QFont font = painter.font();
    font.setPointSize(fontSize);
    font.setBold(true);
    painter.setFont(font);
    const QFontMetricsF metrics(font);

    auto mapPoint = [scale](const QPoint &p) {
        return QPointF(p.x() * scale, p.y() * scale);
    };

    // Counting grid, under the annotations.
    if (m_showGrid && !m_image.isNull()) {
        QPen gridPen(QColor(92, 225, 230, 90));
        gridPen.setWidthF(qMax(1.0, lineWidth / 2.0));
        painter.setPen(gridPen);
        const double step = m_gridCellPx * scale;
        const double w = m_image.width() * scale;
        const double h = m_image.height() * scale;
        for (double x = step; x < w; x += step)
            painter.drawLine(QPointF(x, 0), QPointF(x, h));
        for (double y = step; y < h; y += step)
            painter.drawLine(QPointF(0, y), QPointF(w, y));
    }

    // Measures: line with end ticks + a readable value label at the middle.
    for (const auto &measure : m_measures) {
        const QPointF a = mapPoint(measure.first);
        const QPointF b = mapPoint(measure.second);
        QPen measurePen(measureColor);
        measurePen.setWidthF(lineWidth);
        painter.setPen(measurePen);
        painter.drawLine(a, b);
        painter.setBrush(measureColor);
        painter.drawEllipse(a, lineWidth * 1.6, lineWidth * 1.6);
        painter.drawEllipse(b, lineWidth * 1.6, lineWidth * 1.6);

        const QString text = measureText(measure.first, measure.second);
        const QPointF mid((a.x() + b.x()) / 2.0, (a.y() + b.y()) / 2.0);
        const QRectF textRect(mid.x() - metrics.horizontalAdvance(text) / 2.0 - 4.0,
                               mid.y() - metrics.height() - 6.0,
                               metrics.horizontalAdvance(text) + 8.0, metrics.height() + 4.0);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(5, 7, 10, 200));
        painter.drawRoundedRect(textRect, 4.0, 4.0);
        painter.setPen(measureColor);
        painter.drawText(textRect, Qt::AlignCenter, text);
    }

    // In-progress measure drag (screen only).
    if (!forExport && m_dragging) {
        const QPointF a = mapPoint(m_dragStart);
        const QPointF b = mapPoint(m_dragCurrent);
        QPen dragPen(measureColor);
        dragPen.setWidthF(lineWidth);
        dragPen.setStyle(Qt::DashLine);
        painter.setPen(dragPen);
        painter.drawLine(a, b);
        const QString text = measureText(m_dragStart, m_dragCurrent);
        painter.drawText(QPointF(b.x() + 10.0, b.y() - 10.0), text);
    }

    // Numbered counting markers on top.
    for (int i = 0; i < m_markers.size(); ++i) {
        const QPointF center = mapPoint(m_markers.at(i));
        painter.setPen(Qt::NoPen);
        painter.setBrush(accent);
        painter.drawEllipse(center, markerRadius, markerRadius);
        painter.setPen(markerText);
        const QRectF numberRect(center.x() - markerRadius, center.y() - markerRadius,
                                 markerRadius * 2.0, markerRadius * 2.0);
        painter.drawText(numberRect, Qt::AlignCenter, QString::number(i + 1));
    }
}

void AnalysisCanvas::paintEvent(QPaintEvent * /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), QColor("#05070a"));

    if (m_image.isNull()) {
        painter.setPen(QColor("#5ce1e6"));
        QFont font = painter.font();
        font.setPointSize(13);
        painter.setFont(font);
        painter.drawText(rect(), Qt::AlignCenter,
                          QStringLiteral("Aucune image.\nConnectez la caméra ou ouvrez une image."));
        return;
    }

    const QRect displayRect = imageDisplayRect();
    painter.drawImage(displayRect, m_image);

    painter.save();
    painter.translate(displayRect.topLeft());
    const double scale = static_cast<double>(displayRect.width()) / m_image.width();
    painter.setClipRect(QRectF(0, 0, displayRect.width(), displayRect.height()));
    drawAnnotations(painter, scale, false);
    painter.restore();
}

void AnalysisCanvas::mousePressEvent(QMouseEvent *event)
{
    if (m_image.isNull() || event->button() != Qt::LeftButton)
        return;

    const QPoint imagePoint = widgetToImage(event->pos());
    if (imagePoint.x() < 0)
        return;

    if (m_mode == Mode::Count) {
        m_markers.append(imagePoint);
        m_history.append(ActionKind::Marker);
        update();
        emit annotationsChanged();
    } else {
        m_dragging = true;
        m_dragStart = imagePoint;
        m_dragCurrent = imagePoint;
        update();
    }
}

void AnalysisCanvas::mouseMoveEvent(QMouseEvent *event)
{
    if (!m_dragging)
        return;
    const QPoint imagePoint = widgetToImage(event->pos());
    if (imagePoint.x() < 0)
        return;
    m_dragCurrent = imagePoint;
    update();
}

void AnalysisCanvas::mouseReleaseEvent(QMouseEvent *event)
{
    if (!m_dragging || event->button() != Qt::LeftButton)
        return;
    m_dragging = false;

    const QPoint imagePoint = widgetToImage(event->pos());
    if (imagePoint.x() >= 0)
        m_dragCurrent = imagePoint;

    if (QLineF(m_dragStart, m_dragCurrent).length() >= kMinMeasureLengthPx) {
        m_measures.append(qMakePair(m_dragStart, m_dragCurrent));
        m_history.append(ActionKind::Measure);
        emit annotationsChanged();
    }
    update();
}

QImage AnalysisCanvas::renderAnnotated() const
{
    if (m_image.isNull())
        return {};

    QImage result = m_image.convertToFormat(QImage::Format_RGB32);
    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing);
    drawAnnotations(painter, 1.0, true);
    painter.end();
    return result;
}

AnalysisToolsDialog::AnalysisToolsDialog(AppSettings *settings, const QImage &snapshot, QWidget *parent)
    : QDialog(parent), m_settings(settings)
{
    setWindowTitle(QStringLiteral("Analyse — comptage et mesures"));
    resize(900, 700);

    auto *root = new QVBoxLayout(this);

    auto *instruction = new QLabel(
        QStringLiteral("Mode Compter : un clic pose un marqueur numéroté. Mode Mesurer : cliquez-glissez entre "
                        "deux points pour afficher la distance (selon l'étalonnage de l'échelle)."),
        this);
    instruction->setWordWrap(true);
    root->addWidget(instruction);

    auto *toolsRow = new QHBoxLayout();
    m_countModeButton = new QPushButton(QStringLiteral("Compter"), this);
    m_measureModeButton = new QPushButton(QStringLiteral("Mesurer"), this);
    m_countModeButton->setCheckable(true);
    m_measureModeButton->setCheckable(true);
    m_countModeButton->setChecked(true);
    auto *modeGroup = new QButtonGroup(this);
    modeGroup->setExclusive(true);
    modeGroup->addButton(m_countModeButton);
    modeGroup->addButton(m_measureModeButton);

    auto *gridCheck = new QCheckBox(QStringLiteral("Quadrillage"), this);
    m_gridCellSpin = new QSpinBox(this);
    m_gridCellSpin->setRange(20, 500);
    m_gridCellSpin->setValue(100);
    m_gridCellSpin->setSuffix(QStringLiteral(" px"));
    m_gridCellSpin->setToolTip(QStringLiteral("Taille d'une case du quadrillage, en pixels de l'image"));

    m_countLabel = new QLabel(QStringLiteral("Marqueurs : 0"), this);

    toolsRow->addWidget(m_countModeButton);
    toolsRow->addWidget(m_measureModeButton);
    toolsRow->addSpacing(24);
    toolsRow->addWidget(gridCheck);
    toolsRow->addWidget(m_gridCellSpin);
    toolsRow->addStretch();
    toolsRow->addWidget(m_countLabel);
    root->addLayout(toolsRow);

    m_canvas = new AnalysisCanvas(this);
    m_canvas->setImage(snapshot);
    m_canvas->setMicronsPer100Px(m_settings->scaleBarMicronsPer100Px());
    root->addWidget(m_canvas, 1);

    auto *scaleInfo = new QLabel(
        QStringLiteral("Échelle actuelle : %1 µm pour 100 px — étalonnable dans Réglages avancés.")
            .arg(m_settings->scaleBarMicronsPer100Px(), 0, 'f', 1),
        this);
    root->addWidget(scaleInfo);

    auto *buttonRow = new QHBoxLayout();
    auto *openButton = new QPushButton(QStringLiteral("Ouvrir une image..."), this);
    auto *undoButton = new QPushButton(QStringLiteral("Annuler le dernier"), this);
    auto *clearButton = new QPushButton(QStringLiteral("Tout effacer"), this);
    auto *saveButton = new QPushButton(QStringLiteral("Enregistrer l'image annotée..."), this);
    auto *closeButton = new QPushButton(QStringLiteral("Fermer"), this);
    buttonRow->addWidget(openButton);
    buttonRow->addWidget(undoButton);
    buttonRow->addWidget(clearButton);
    buttonRow->addStretch();
    buttonRow->addWidget(saveButton);
    buttonRow->addWidget(closeButton);
    root->addLayout(buttonRow);

    connect(m_countModeButton, &QPushButton::toggled, this, [this](bool checked) {
        if (checked)
            m_canvas->setMode(AnalysisCanvas::Mode::Count);
    });
    connect(m_measureModeButton, &QPushButton::toggled, this, [this](bool checked) {
        if (checked)
            m_canvas->setMode(AnalysisCanvas::Mode::Measure);
    });
    connect(gridCheck, &QCheckBox::toggled, m_canvas, &AnalysisCanvas::setShowGrid);
    connect(m_gridCellSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            m_canvas, &AnalysisCanvas::setGridCellPx);
    connect(m_canvas, &AnalysisCanvas::annotationsChanged, this, &AnalysisToolsDialog::onAnnotationsChanged);
    connect(openButton, &QPushButton::clicked, this, &AnalysisToolsDialog::onOpenImage);
    connect(undoButton, &QPushButton::clicked, m_canvas, &AnalysisCanvas::undoLast);
    connect(clearButton, &QPushButton::clicked, m_canvas, &AnalysisCanvas::clearAll);
    connect(saveButton, &QPushButton::clicked, this, &AnalysisToolsDialog::onSaveAnnotated);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
}

void AnalysisToolsDialog::onOpenImage()
{
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Ouvrir une image"),
        m_settings->activeCaptureFolder(),
        QStringLiteral("Images (*.png *.jpg *.jpeg *.tif *.tiff *.bmp)"));
    if (path.isEmpty())
        return;

    const QImage image(path);
    if (image.isNull()) {
        QMessageBox::warning(this, QStringLiteral("Analyse"),
                              QStringLiteral("Impossible d'ouvrir cette image."));
        return;
    }
    m_canvas->setImage(image);
}

void AnalysisToolsDialog::onSaveAnnotated()
{
    if (!m_canvas->hasImage()) {
        QMessageBox::information(this, QStringLiteral("Analyse"),
                                  QStringLiteral("Aucune image à enregistrer."));
        return;
    }

    const QString defaultName = QStringLiteral("analyse_%1.png")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Enregistrer l'image annotée"),
        QDir(m_settings->activeCaptureFolder()).filePath(defaultName),
        QStringLiteral("Image PNG (*.png)"));
    if (path.isEmpty())
        return;

    if (!m_canvas->renderAnnotated().save(path)) {
        QMessageBox::warning(this, QStringLiteral("Analyse"),
                              QStringLiteral("Échec de l'enregistrement de l'image annotée."));
        return;
    }
    QMessageBox::information(this, QStringLiteral("Analyse"),
                              QStringLiteral("Image annotée enregistrée."));
}

void AnalysisToolsDialog::onAnnotationsChanged()
{
    m_countLabel->setText(QStringLiteral("Marqueurs : %1").arg(m_canvas->markerCount()));
}
