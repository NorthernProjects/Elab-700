#include "TopStatusBar.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QPixmap>
#include <QStyle>
#if defined(Q_OS_MAC)
#include <QMouseEvent>
#include <QWindow>
#endif

TopStatusBar::TopStatusBar(QWidget *parent) : QWidget(parent)
{
    setObjectName("TopStatusBar");

    m_logoLabel = new QLabel(this);
    m_logoLabel->setObjectName("logoLabel");
    const QPixmap logo(QStringLiteral(":/branding/logo_no_tagline.png"));
    if (!logo.isNull())
        m_logoLabel->setPixmap(logo.scaledToHeight(32, Qt::SmoothTransformation));

    // A QPushButton (not QLabel) — clicking it opens a menu listing every
    // detected camera (connected or not) so a teacher can force-connect to
    // a specific one if auto-detection picked the wrong device or none.
    m_connectionButton = new QPushButton(QStringLiteral("● Caméra non connectée"));
    m_connectionButton->setObjectName("connectionLabel");
    m_connectionButton->setFlat(true);
    m_connectionButton->setCursor(Qt::PointingHandCursor);
    m_connectionButton->setToolTip(QStringLiteral("Voir/choisir la caméra"));

    m_fpsLabel = new QLabel(QStringLiteral("-- ips"));
    m_fpsLabel->setObjectName("fpsLabel");

    // A QPushButton (not QLabel), like #microscopeLabel — clicking it opens
    // a menu to pick the camera resolution directly from the main screen,
    // instead of only via the teacher panel.
    m_resolutionButton = new QPushButton(QStringLiteral("--x--"));
    m_resolutionButton->setObjectName("resolutionLabel");
    m_resolutionButton->setFlat(true);
    m_resolutionButton->setCursor(Qt::PointingHandCursor);
    m_resolutionButton->setToolTip(QStringLiteral("Choisir la résolution de la caméra"));

    // A QPushButton (not QLabel) so the title is clickable to show the
    // microscope/camera specs panel; #microscopeLabel QSS strips the
    // default button chrome so it still reads as plain text. No hardcoded
    // model — setMicroscopeName() fills in whatever the user typed in the
    // settings (or leaves it plain).
    m_microscopeLabel = new QPushButton(QStringLiteral("E-Lab 700"), this);
    m_microscopeLabel->setObjectName("microscopeLabel");
    m_microscopeLabel->setCursor(Qt::PointingHandCursor);
    m_microscopeLabel->setToolTip(QStringLiteral("Voir les caractéristiques du microscope"));
    m_microscopeLabel->setFlat(true);

    m_groupButton = new QPushButton(QStringLiteral("Se connecter"), this);
    m_groupButton->setObjectName("groupButton");
    m_groupButton->setCursor(Qt::PointingHandCursor);
    m_groupButton->setToolTip(QStringLiteral("Choisir ta classe et ton groupe"));
    // Only meaningful when the classes/groups feature is on — MainWindow
    // drives visibility via setGroupButtonVisible() from the feature flags.
    m_groupButton->setVisible(false);

    m_teacherButton = new QPushButton(QStringLiteral("⚙"));
    m_teacherButton->setObjectName("teacherButton");
    m_teacherButton->setFixedSize(36, 36);
    m_teacherButton->setToolTip(QStringLiteral("Réglages"));
    m_teacherButton->setCursor(Qt::PointingHandCursor);

    m_helpButton = new QPushButton(QStringLiteral("?"));
    m_helpButton->setObjectName("teacherButton");
    m_helpButton->setFixedSize(36, 36);
    m_helpButton->setToolTip(QStringLiteral("Aide"));
    m_helpButton->setCursor(Qt::PointingHandCursor);

    // Left/center/right built as separate containers placed in a 3-column
    // grid with equal outer stretch (see BottomBar for the same technique):
    // this keeps the center column (software/microscope name) at the true
    // horizontal center regardless of how wide the left or right content
    // ends up being. The group login button lives on the right, next to the
    // teacher button, per the classroom layout the software mirrors.
    auto *leftContainer = new QWidget(this);
    auto *leftLayout = new QHBoxLayout(leftContainer);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->addWidget(m_logoLabel);
    leftLayout->addSpacing(16);
    leftLayout->addWidget(m_connectionButton);
    leftLayout->addSpacing(24);
    leftLayout->addWidget(m_resolutionButton);
    leftLayout->addSpacing(24);
    leftLayout->addWidget(m_fpsLabel);

    auto *rightContainer = new QWidget(this);
    auto *rightLayout = new QHBoxLayout(rightContainer);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(12);
    rightLayout->addWidget(m_groupButton);
    rightLayout->addWidget(m_helpButton);
    rightLayout->addWidget(m_teacherButton);

    m_layout = new QGridLayout(this);
    m_layout->setContentsMargins(m_baseLeftMargin, 8, 16, 8);
    m_layout->setColumnStretch(0, 1);
    m_layout->setColumnStretch(1, 0);
    m_layout->setColumnStretch(2, 1);
    m_layout->addWidget(leftContainer, 0, 0, Qt::AlignLeft | Qt::AlignVCenter);
    m_layout->addWidget(m_microscopeLabel, 0, 1, Qt::AlignCenter);
    m_layout->addWidget(rightContainer, 0, 2, Qt::AlignRight | Qt::AlignVCenter);

    connect(m_teacherButton, &QPushButton::clicked, this, &TopStatusBar::teacherModeRequested);
    connect(m_groupButton, &QPushButton::clicked, this, &TopStatusBar::groupSelectionRequested);
    connect(m_microscopeLabel, &QPushButton::clicked, this, &TopStatusBar::microscopeInfoRequested);
    connect(m_resolutionButton, &QPushButton::clicked, this, &TopStatusBar::resolutionClicked);
    connect(m_connectionButton, &QPushButton::clicked, this, &TopStatusBar::connectionClicked);
    connect(m_helpButton, &QPushButton::clicked, this, &TopStatusBar::helpRequested);
}

void TopStatusBar::setConnected(bool connected, const QString &modelName)
{
    if (connected) {
        m_connectionButton->setText(modelName.isEmpty()
            ? QStringLiteral("● Caméra connectée")
            : QStringLiteral("● Caméra connectée (%1)").arg(modelName));
        m_connectionButton->setProperty("connected", true);
    } else {
        m_connectionButton->setText(QStringLiteral("● Caméra non connectée"));
        m_connectionButton->setProperty("connected", false);
        m_fpsLabel->setText(QStringLiteral("-- ips"));
        m_resolutionButton->setText(QStringLiteral("--x--"));
    }
    m_connectionButton->style()->unpolish(m_connectionButton);
    m_connectionButton->style()->polish(m_connectionButton);
}

void TopStatusBar::setFps(double fps)
{
    m_fpsLabel->setText(QStringLiteral("%1 ips").arg(fps, 0, 'f', 1));
}

void TopStatusBar::setResolution(const QSize &size)
{
    if (size.isEmpty()) {
        m_resolutionButton->setText(QStringLiteral("--x--"));
    } else {
        m_resolutionButton->setText(QStringLiteral("%1x%2").arg(size.width()).arg(size.height()));
    }
}

void TopStatusBar::setGroupButtonVisible(bool visible)
{
    m_groupButton->setVisible(visible);
}

void TopStatusBar::setTeacherButtonToolTip(const QString &tip)
{
    m_teacherButton->setToolTip(tip);
}

void TopStatusBar::setMicroscopeName(const QString &name)
{
    const QString trimmed = name.trimmed();
    m_microscopeLabel->setText(trimmed.isEmpty()
        ? QStringLiteral("E-Lab 700")
        : QStringLiteral("E-Lab 700 · %1").arg(trimmed));
}

#if defined(Q_OS_MAC)
void TopStatusBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (QWindow *handle = window()->windowHandle()) {
            handle->startSystemMove();
            event->accept();
            return;
        }
    }
    QWidget::mousePressEvent(event);
}
#endif

void TopStatusBar::setLeftInset(int pixels)
{
    QMargins margins = m_layout->contentsMargins();
    margins.setLeft(m_baseLeftMargin + pixels);
    m_layout->setContentsMargins(margins);
}

void TopStatusBar::setGroupInfo(const QString &className, const QString &groupName)
{
    if (className.isEmpty() && groupName.isEmpty()) {
        m_groupButton->setText(QStringLiteral("Se connecter"));
        return;
    }
    m_groupButton->setText(QStringLiteral("%1 — %2").arg(className, groupName));
}
