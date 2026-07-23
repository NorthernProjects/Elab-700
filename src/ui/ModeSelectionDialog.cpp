#include "ModeSelectionDialog.h"

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

ModeSelectionDialog::ModeSelectionDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Bienvenue dans E-Lab 700"));
    setModal(true);
    setMinimumWidth(560);

    auto *root = new QVBoxLayout(this);
    root->setSpacing(12);

    auto *intro = new QLabel(
        QStringLiteral("<b>E-Lab 700 — microscopie numérique</b><br>"
                        "La version de base est adaptée au grand public, au scolaire et au laboratoire.<br>"
                        "Choisissez votre édition (gratuite, modifiable à tout moment dans les réglages) :"),
        this);
    intro->setWordWrap(true);
    root->addWidget(intro);

    addChoice(QStringLiteral("school"), QStringLiteral("🏫  Scolaire (salle de classe)"),
              QStringLiteral("Élèves et professeur : classes et groupes avec galeries séparées, code PIN "
                              "professeur, minuteur d'observation, envoi des photos à l'enseignant, schéma du "
                              "microscope et glossaire."));
    addChoice(QStringLiteral("public"), QStringLiteral("🏠  Grand public"),
              QStringLiteral("Simple et complet : photo, vidéo, time-lapse, galerie, zoom, analyse (comptage et "
                              "mesures), schéma du microscope et glossaire pour apprendre."));
    addChoice(QStringLiteral("lab"), QStringLiteral("🔬  Laboratoire"),
              QStringLiteral("Orienté pro : analyse (comptage, mesures en µm/mm, quadrillage), format photo "
                              "TIFF/PNG/JPG, métadonnées de capture, étalonnage de l'échelle."));

    auto *note = new QLabel(
        QStringLiteral("Chaque édition active les options qui lui vont bien — et les réglages permettent "
                        "d'ajouter à votre édition les fonctionnalités des autres."),
        this);
    note->setWordWrap(true);
    root->addWidget(note);
}

void ModeSelectionDialog::addChoice(const QString &mode, const QString &title, const QString &description)
{
    auto *button = new QPushButton(this);
    button->setCursor(Qt::PointingHandCursor);
    button->setMinimumHeight(84);

    // Rich two-line content inside the button via a child label (QPushButton
    // itself renders single-line plain text only).
    auto *inner = new QLabel(QStringLiteral("<b>%1</b><br><span style='font-size:11px'>%2</span>")
                                  .arg(title, description),
                              button);
    inner->setWordWrap(true);
    inner->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    auto *innerLayout = new QVBoxLayout(button);
    innerLayout->setContentsMargins(16, 8, 16, 8);
    innerLayout->addWidget(inner);

    connect(button, &QPushButton::clicked, this, [this, mode]() {
        m_selectedMode = mode;
        accept();
    });

    static_cast<QVBoxLayout *>(layout())->addWidget(button);
}
