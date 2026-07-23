#include "ScaleCalibrationDialog.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineF>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>

CalibrationImageWidget::CalibrationImageWidget(QWidget *parent) : QWidget(parent)
{
    setMinimumSize(320, 240);
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor("#05070a"));
    setPalette(pal);
}

void CalibrationImageWidget::setImage(const QImage &image)
{
    m_image = image;
    m_points.clear();
    update();
}

void CalibrationImageWidget::resetPoints()
{
    m_points.clear();
    update();
    emit pointsChanged();
}

double CalibrationImageWidget::pixelDistance() const
{
    if (m_points.size() != 2)
        return 0.0;
    return QLineF(m_points.at(0), m_points.at(1)).length();
}

QRect CalibrationImageWidget::imageDisplayRect() const
{
    if (m_image.isNull())
        return rect();
    const QSize target = m_image.size().scaled(size(), Qt::KeepAspectRatio);
    return QRect(QPoint((width() - target.width()) / 2, (height() - target.height()) / 2), target);
}

QPoint CalibrationImageWidget::widgetToImage(const QPoint &widgetPoint) const
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

void CalibrationImageWidget::paintEvent(QPaintEvent * /*event*/)
{
    QPainter painter(this);
    painter.fillRect(rect(), QColor("#05070a"));

    if (m_image.isNull())
        return;

    const QRect displayRect = imageDisplayRect();
    painter.drawImage(displayRect, m_image);

    if (m_points.isEmpty())
        return;

    const double sx = static_cast<double>(displayRect.width()) / m_image.width();
    const double sy = static_cast<double>(displayRect.height()) / m_image.height();
    auto toWidget = [&](const QPoint &imagePoint) {
        return QPoint(displayRect.left() + static_cast<int>(imagePoint.x() * sx),
                       displayRect.top() + static_cast<int>(imagePoint.y() * sy));
    };

    QPen pen(QColor("#5ce1e6"));
    pen.setWidth(3);
    painter.setPen(pen);
    painter.setBrush(QColor("#5ce1e6"));

    QVector<QPoint> widgetPoints;
    for (const QPoint &p : m_points) {
        const QPoint wp = toWidget(p);
        widgetPoints.append(wp);
        painter.drawEllipse(wp, 6, 6);
    }
    if (widgetPoints.size() == 2)
        painter.drawLine(widgetPoints.at(0), widgetPoints.at(1));
}

void CalibrationImageWidget::mousePressEvent(QMouseEvent *event)
{
    if (m_image.isNull() || event->button() != Qt::LeftButton)
        return;

    const QPoint imagePoint = widgetToImage(event->pos());
    if (imagePoint.x() < 0)
        return;

    if (m_points.size() >= 2)
        m_points.clear();
    m_points.append(imagePoint);
    update();
    emit pointsChanged();
}

ScaleCalibrationDialog::ScaleCalibrationDialog(const QImage &snapshot, AppSettings *settings, QWidget *parent)
    : QDialog(parent), m_settings(settings)
{
    setWindowTitle(QStringLiteral("Étalonner l'échelle de mesure"));
    resize(820, 640);

    auto *root = new QVBoxLayout(this);

    m_instructionLabel = new QLabel(
        QStringLiteral("Placez un objet de taille connue sous le microscope (une règle, une lame micrométrique...), "
                        "puis cliquez sur deux points dont vous connaissez la distance réelle."),
        this);
    m_instructionLabel->setWordWrap(true);
    root->addWidget(m_instructionLabel);

    m_canvas = new CalibrationImageWidget(this);
    m_canvas->setImage(snapshot);
    root->addWidget(m_canvas, 1);

    auto *distanceRow = new QHBoxLayout();
    auto *distanceLabel = new QLabel(QStringLiteral("Distance réelle entre les deux points :"), this);
    m_realDistanceSpin = new QDoubleSpinBox(this);
    m_realDistanceSpin->setRange(0.001, 1000000.0);
    m_realDistanceSpin->setDecimals(3);
    m_realDistanceSpin->setValue(1.0);
    m_unitCombo = new QComboBox(this);
    m_unitCombo->addItem(QStringLiteral("mm"), 1000.0);
    m_unitCombo->addItem(QStringLiteral("µm"), 1.0);
    distanceRow->addWidget(distanceLabel);
    distanceRow->addWidget(m_realDistanceSpin);
    distanceRow->addWidget(m_unitCombo);
    distanceRow->addStretch();
    root->addLayout(distanceRow);

    auto *buttonRow = new QHBoxLayout();
    auto *resetButton = new QPushButton(QStringLiteral("Recommencer"), this);
    auto *cancelButton = new QPushButton(QStringLiteral("Annuler"), this);
    m_confirmButton = new QPushButton(QStringLiteral("Confirmer l'étalonnage"), this);
    m_confirmButton->setEnabled(false);
    buttonRow->addWidget(resetButton);
    buttonRow->addStretch();
    buttonRow->addWidget(cancelButton);
    buttonRow->addWidget(m_confirmButton);
    root->addLayout(buttonRow);

    connect(m_canvas, &CalibrationImageWidget::pointsChanged, this, &ScaleCalibrationDialog::onPointsChanged);
    connect(resetButton, &QPushButton::clicked, this, &ScaleCalibrationDialog::onReset);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_confirmButton, &QPushButton::clicked, this, &ScaleCalibrationDialog::onConfirm);
}

void ScaleCalibrationDialog::onPointsChanged()
{
    m_confirmButton->setEnabled(m_canvas->hasTwoPoints());
}

void ScaleCalibrationDialog::onReset()
{
    m_canvas->resetPoints();
}

void ScaleCalibrationDialog::onConfirm()
{
    const double pixelDist = m_canvas->pixelDistance();
    if (pixelDist <= 0.0) {
        QMessageBox::warning(this, QStringLiteral("Étalonnage"),
                              QStringLiteral("Cliquez sur deux points distincts avant de confirmer."));
        return;
    }

    const double unitFactor = m_unitCombo->currentData().toDouble(); // -> microns
    const double realDistanceMicrons = m_realDistanceSpin->value() * unitFactor;
    const double micronsPer100Px = (realDistanceMicrons / pixelDist) * 100.0;

    m_settings->setScaleBarMicronsPer100Px(micronsPer100Px);
    m_settings->setShowScaleBar(true);

    QMessageBox::information(this, QStringLiteral("Étalonnage"),
                              QStringLiteral("Échelle mise à jour : %1 µm pour 100 px.")
                                  .arg(micronsPer100Px, 0, 'f', 1));
    accept();
}
