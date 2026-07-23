#include "MicroscopeInfoPanel.h"

#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "GlossaryDialog.h"
#include "MicroscopeDiagramDialog.h"

MicroscopeInfoPanel::MicroscopeInfoPanel(QWidget *parent) : QWidget(parent)
{
    setObjectName("microscopeInfoPanel");
    setAttribute(Qt::WA_StyledBackground, true);
    setFixedWidth(300);

    auto *shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(28);
    shadow->setOffset(0, 6);
    shadow->setColor(QColor(0, 0, 0, 140));
    setGraphicsEffect(shadow);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 16, 18, 16);
    layout->setSpacing(10);

    auto *headerRow = new QHBoxLayout();
    m_titleLabel = new QLabel(QStringLiteral("Microscope"), this);
    m_titleLabel->setObjectName("microscopeInfoTitle");
    auto *closeButton = new QPushButton(QStringLiteral("✕"), this);
    closeButton->setObjectName("microscopeInfoClose");
    closeButton->setFixedSize(24, 24);
    closeButton->setCursor(Qt::PointingHandCursor);
    headerRow->addWidget(m_titleLabel);
    headerRow->addStretch(1);
    headerRow->addWidget(closeButton);
    layout->addLayout(headerRow);

    // The user's microscope is whatever they own — no hardcoded spec sheet.
    auto *subtitle = new QLabel(QStringLiteral("Microscopie numérique"), this);
    subtitle->setObjectName("microscopeInfoSubtitle");
    subtitle->setWordWrap(true);
    layout->addWidget(subtitle);

    auto *cameraTitle = new QLabel(QStringLiteral("<b>Caméra</b>"), this);
    cameraTitle->setObjectName("microscopeInfoSection");
    layout->addWidget(cameraTitle);

    auto addRow = [&](const QString &label, QLabel *&valueLabel) {
        auto *row = new QHBoxLayout();
        auto *labelWidget = new QLabel(label, this);
        labelWidget->setObjectName("microscopeInfoLabel");
        valueLabel = new QLabel(QStringLiteral("--"), this);
        valueLabel->setObjectName("microscopeInfoValue");
        row->addWidget(labelWidget);
        row->addStretch(1);
        row->addWidget(valueLabel);
        layout->addLayout(row);
    };

    addRow(QStringLiteral("Connexion"), m_connectionValue);
    addRow(QStringLiteral("Résolution"), m_resolutionValue);
    addRow(QStringLiteral("Images/seconde"), m_fpsValue);

    connect(closeButton, &QPushButton::clicked, this, &MicroscopeInfoPanel::closeRequested);

    // Learning aids (diagram + glossary): shown or hidden at runtime from
    // the "learning aids" feature flag — on for the school and grand-public
    // editions, off by default for the lab edition, always re-toggleable.
    m_diagramButton = new QPushButton(QStringLiteral("Voir le schéma du microscope"), this);
    m_diagramButton->setCursor(Qt::PointingHandCursor);
    layout->addWidget(m_diagramButton);

    m_glossaryButton = new QPushButton(QStringLiteral("Voir le glossaire"), this);
    m_glossaryButton->setCursor(Qt::PointingHandCursor);
    layout->addWidget(m_glossaryButton);

    connect(m_diagramButton, &QPushButton::clicked, this, [this]() {
        MicroscopeDiagramDialog dialog(this);
        dialog.exec();
    });
    connect(m_glossaryButton, &QPushButton::clicked, this, [this]() {
        GlossaryDialog dialog(this);
        dialog.exec();
    });
}

void MicroscopeInfoPanel::setLearningAidsVisible(bool visible)
{
    m_diagramButton->setVisible(visible);
    m_glossaryButton->setVisible(visible);
}

void MicroscopeInfoPanel::setMicroscopeName(const QString &name)
{
    const QString trimmed = name.trimmed();
    m_titleLabel->setText(trimmed.isEmpty() ? QStringLiteral("Microscope") : trimmed);
}

void MicroscopeInfoPanel::setResolution(const QSize &size)
{
    m_resolutionValue->setText(size.isEmpty()
        ? QStringLiteral("--x--")
        : QStringLiteral("%1 x %2").arg(size.width()).arg(size.height()));
}

void MicroscopeInfoPanel::setFps(double fps)
{
    m_fpsValue->setText(QStringLiteral("%1 ips").arg(fps, 0, 'f', 1));
}

void MicroscopeInfoPanel::setConnected(bool connected)
{
    m_connectionValue->setText(connected ? QStringLiteral("USB (active)") : QStringLiteral("Non connectée"));
}
