#include "MicroscopeDiagramDialog.h"

#include <algorithm>

#include <QFont>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>

namespace {

void drawLeaderLabel(QPainter &painter, const QPointF &anchor, const QPointF &labelPos,
                      const QString &text, const QColor &lineColor)
{
    // labelPos = left edge of the text, vertically centered. The leader line
    // must attach to whichever text edge FACES the anchor — measured from
    // the real rendered width — never to a point on the far side, which
    // made the line run underneath the words (the earlier bug: labels left
    // of the drawing got their line drawn to a point left of the text,
    // crossing the whole label on the way there).
    QFont font = painter.font();
    font.setPointSize(11);
    painter.setFont(font);
    const QFontMetricsF metrics(font);
    const qreal textWidth = metrics.horizontalAdvance(text);
    constexpr qreal kTextGap = 7.0;

    QPointF lineEnd;
    if (anchor.x() >= labelPos.x() + textWidth / 2.0)
        lineEnd = QPointF(labelPos.x() + textWidth + kTextGap, labelPos.y());
    else
        lineEnd = QPointF(labelPos.x() - kTextGap, labelPos.y());

    QPen pen(lineColor);
    pen.setWidth(1);
    pen.setStyle(Qt::DashLine);
    painter.setPen(pen);
    painter.drawLine(anchor, lineEnd);

    painter.setPen(QColor("#e6f7ff"));
    const QRectF textRect(labelPos.x(), labelPos.y() - 9.0, textWidth + 4.0, 18.0);
    painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, text);
}

} // namespace

MicroscopeDiagramWidget::MicroscopeDiagramWidget(QWidget *parent) : QWidget(parent)
{
    setMinimumSize(420, 560);
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor("#05070a"));
    setPalette(pal);
}

void MicroscopeDiagramWidget::paintEvent(QPaintEvent * /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), QColor("#05070a"));

    constexpr qreal canvasW = 480.0;
    constexpr qreal canvasH = 640.0;
    const qreal scale = std::min(width() / canvasW, height() / canvasH);
    painter.translate((width() - canvasW * scale) / 2.0, (height() - canvasH * scale) / 2.0);
    painter.scale(scale, scale);

    const QColor metal("#4d6a82");
    const QColor metalLight("#7c9bb5");
    const QColor accent("#5ce1e6");
    const QColor dark("#22384d");

    painter.setPen(QPen(dark, 2));

    // Base (socle)
    painter.setBrush(metal);
    painter.drawRoundedRect(QRectF(150, 582, 180, 34), 8, 8);

    // Arm, vertical segment (bras)
    painter.setBrush(metalLight);
    painter.drawRoundedRect(QRectF(296, 220, 46, 372), 16, 16);

    // Arm, top curve connecting to the head
    painter.drawRoundedRect(QRectF(210, 160, 132, 66), 20, 20);

    // Light source (illuminateur), at the base, below the condenser
    painter.setBrush(QColor("#e0c235"));
    painter.drawRoundedRect(QRectF(244, 558, 32, 20), 4, 4);

    // Condenser (condenseur)
    painter.setBrush(metal);
    QPolygonF condenser;
    condenser << QPointF(248, 452) << QPointF(272, 452) << QPointF(266, 430) << QPointF(254, 430);
    painter.drawPolygon(condenser);

    // Stage (platine)
    painter.setBrush(metalLight);
    painter.drawRoundedRect(QRectF(120, 400, 182, 26), 4, 4);
    // Stage light-path hole
    painter.setBrush(QColor("#05070a"));
    painter.drawEllipse(QPointF(260, 413), 9, 9);
    // Stage clips
    painter.setBrush(metal);
    painter.drawRoundedRect(QRectF(200, 396, 14, 12), 2, 2);
    painter.drawRoundedRect(QRectF(300, 396, 14, 12), 2, 2);

    // Stage movement knob (molette de déplacement)
    painter.setBrush(metal);
    painter.drawEllipse(QPointF(318, 430), 12, 12);

    // Nosepiece / turret (tourelle porte-objectifs) with three objectives
    painter.setBrush(metalLight);
    painter.drawEllipse(QPointF(260, 358), 36, 16);
    painter.setBrush(metal);
    painter.drawRoundedRect(QRectF(250, 366, 20, 38), 4, 4);   // main objective, aimed at the stage hole
    painter.drawRoundedRect(QRectF(222, 358, 14, 22), 3, 3);   // second objective (partially visible, turned away)
    painter.drawRoundedRect(QRectF(288, 358, 14, 22), 3, 3);   // third objective

    // Body tube (tube), rising from the nosepiece to the head
    painter.setBrush(metalLight);
    painter.drawRoundedRect(QRectF(246, 226, 28, 132), 4, 4);

    // Trinocular head (tête trinoculaire)
    painter.setBrush(metal);
    painter.drawRoundedRect(QRectF(196, 132, 150, 36), 10, 10);

    // Eyepieces (oculaires) — two angled tubes toward the viewer
    painter.setBrush(metalLight);
    painter.save();
    painter.translate(214, 140);
    painter.rotate(-25);
    painter.drawRoundedRect(QRectF(-10, -64, 20, 66), 6, 6);
    painter.restore();
    painter.save();
    painter.translate(238, 140);
    painter.rotate(-15);
    painter.drawRoundedRect(QRectF(-10, -64, 20, 66), 6, 6);
    painter.restore();

    // Camera port + the USB camera itself (straight up from the head)
    painter.setBrush(metalLight);
    painter.drawRoundedRect(QRectF(304, 92, 24, 42), 4, 4);
    painter.setBrush(QColor("#1c2a3a"));
    painter.drawRoundedRect(QRectF(292, 58, 48, 38), 6, 6);
    painter.setBrush(accent);
    painter.drawEllipse(QPointF(316, 77), 10, 10);

    // Focus knobs (vis macrométrique + micrométrique)
    painter.setBrush(metal);
    painter.drawEllipse(QPointF(295, 470), 22, 22);
    painter.setBrush(metalLight);
    painter.drawEllipse(QPointF(295, 470), 10, 10);
    painter.setBrush(metal);
    painter.drawEllipse(QPointF(295, 520), 15, 15);

    // Labels with leader lines — grouped left / right / top so text never
    // overlaps the drawing itself.
    drawLeaderLabel(painter, QPointF(196, 132), QPointF(40, 60),
                    QStringLiteral("Oculaires"), accent);
    drawLeaderLabel(painter, QPointF(328, 77), QPointF(360, 40),
                    QStringLiteral("Caméra (port trinoculaire)"), accent);
    drawLeaderLabel(painter, QPointF(271, 150), QPointF(40, 130),
                    QStringLiteral("Tête trinoculaire"), accent);
    drawLeaderLabel(painter, QPointF(260, 292), QPointF(360, 260),
                    QStringLiteral("Tube optique"), accent);
    drawLeaderLabel(painter, QPointF(260, 358), QPointF(40, 340),
                    QStringLiteral("Tourelle porte-objectifs"), accent);
    drawLeaderLabel(painter, QPointF(250, 380), QPointF(40, 400),
                    QStringLiteral("Objectifs"), accent);
    drawLeaderLabel(painter, QPointF(150, 413), QPointF(40, 470),
                    QStringLiteral("Platine (porte-lame)"), accent);
    drawLeaderLabel(painter, QPointF(260, 440), QPointF(360, 500),
                    QStringLiteral("Condenseur"), accent);
    drawLeaderLabel(painter, QPointF(260, 568), QPointF(40, 560),
                    QStringLiteral("Source lumineuse"), accent);
    drawLeaderLabel(painter, QPointF(319, 220), QPointF(360, 200),
                    QStringLiteral("Bras"), accent);
    drawLeaderLabel(painter, QPointF(295, 470), QPointF(360, 470),
                    QStringLiteral("Vis macrométrique"), accent);
    drawLeaderLabel(painter, QPointF(295, 520), QPointF(360, 530),
                    QStringLiteral("Vis micrométrique"), accent);
    drawLeaderLabel(painter, QPointF(240, 599), QPointF(40, 600),
                    QStringLiteral("Socle (base)"), accent);
}

MicroscopeDiagramDialog::MicroscopeDiagramDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Schéma du microscope"));
    resize(760, 720);

    auto *root = new QVBoxLayout(this);

    auto *introLabel = new QLabel(
        QStringLiteral("Schéma simplifié d'un microscope trinoculaire — les noms des différentes parties, "
                        "pour apprendre à s'en servir."),
        this);
    introLabel->setWordWrap(true);
    root->addWidget(introLabel);

    auto *diagram = new MicroscopeDiagramWidget(this);
    root->addWidget(diagram, 1);

    auto *closeButton = new QPushButton(QStringLiteral("Fermer"), this);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    root->addWidget(closeButton);
}
