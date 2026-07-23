#include "HelpDialog.h"

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include "SmoothScrollArea.h"
#include "core/AppSettings.h"

namespace {

QWidget *makeHelpRow(QWidget *parent, const QString &icon, const QString &title, const QString &description)
{
    auto *row = new QWidget(parent);
    auto *layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 8, 0, 8);

    auto *iconLabel = new QLabel(icon, row);
    QFont iconFont = iconLabel->font();
    iconFont.setPointSize(20);
    iconLabel->setFont(iconFont);
    iconLabel->setFixedWidth(48);
    iconLabel->setAlignment(Qt::AlignCenter);

    auto *textLabel = new QLabel(QStringLiteral("<b>%1</b><br>%2").arg(title, description), row);
    textLabel->setWordWrap(true);

    layout->addWidget(iconLabel);
    layout->addWidget(textLabel, 1);
    return row;
}

} // namespace

HelpDialog::HelpDialog(QWidget *parent) : QDialog(parent)
{
    // Thin QSettings wrapper — instantiating one here just to read the
    // runtime feature flags is cheap and keeps the dialog self-contained.
    AppSettings settings;

    setWindowTitle(QStringLiteral("Aide"));
    resize(560, 620);

    auto *root = new QVBoxLayout(this);

    auto *scrollArea = new SmoothScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    root->addWidget(scrollArea, 1);

    auto *content = new QWidget(scrollArea);
    scrollArea->setWidget(content);
    auto *contentLayout = new QVBoxLayout(content);

    contentLayout->addWidget(makeHelpRow(content, QStringLiteral("📷"), QStringLiteral("Photo"),
        QStringLiteral("Prend une photo de ce que montre le microscope en ce moment.")));
    contentLayout->addWidget(makeHelpRow(content, QStringLiteral("🎥"), QStringLiteral("Vidéo"),
        QStringLiteral("Démarre l'enregistrement d'une vidéo. Le bouton devient rouge (Stop) pendant l'enregistrement.")));
    contentLayout->addWidget(makeHelpRow(content, QStringLiteral("✨"), QStringLiteral("Auto"),
        QStringLiteral("Règle automatiquement l'exposition, la balance des blancs et la résolution de la caméra.")));
    contentLayout->addWidget(makeHelpRow(content, QStringLiteral("🖼"), QStringLiteral("Galerie"),
        QStringLiteral("Retrouve toutes les photos et vidéos prises par ton groupe.")));
    contentLayout->addWidget(makeHelpRow(content, QStringLiteral("⛶"), QStringLiteral("Plein écran"),
        QStringLiteral("Agrandit le logiciel à tout l'écran. Double-clique sur l'image pour un plein écran encore "
                        "plus grand, sans aucun bouton — appuie sur Échap pour revenir.")));
    contentLayout->addWidget(makeHelpRow(content, QStringLiteral("−ㅤ+"), QStringLiteral("Zoom"),
        QStringLiteral("Zoome ou dézoome l'image affichée. Clique sur le pourcentage au milieu pour revenir à 100%.")));
    contentLayout->addWidget(makeHelpRow(content, QStringLiteral("●"), QStringLiteral("Netteté"),
        QStringLiteral("Petite barre en haut à gauche de l'image : verte si l'image est nette, rouge si elle est floue "
                        "— tourne la molette de mise au point du microscope jusqu'à ce qu'elle devienne verte.")));
    contentLayout->addWidget(makeHelpRow(content, QStringLiteral("▦"), QStringLiteral("Grille"),
        QStringLiteral("Affiche une grille sur l'image pour mieux cadrer une photo. Clique sur le badge pour "
                        "l'activer ou la désactiver (vert = activée, rouge = désactivée).")));
    contentLayout->addWidget(makeHelpRow(content, QStringLiteral("📏"), QStringLiteral("Échelle de mesure"),
        QStringLiteral("Une règle affichée sur l'image pour estimer la taille de ce qu'on observe.")));
    contentLayout->addWidget(makeHelpRow(content, QStringLiteral("●"), QStringLiteral("Caméra connectée / non connectée"),
        QStringLiteral("En haut à gauche. Clique dessus pour voir toutes les caméras détectées et choisir laquelle "
                        "utiliser, ou pour déconnecter la caméra proprement.")));
    contentLayout->addWidget(makeHelpRow(content, QStringLiteral("🔎"), QStringLiteral("Résolution"),
        QStringLiteral("Clique sur le chiffre (par exemple 1280x960) pour choisir une autre résolution de caméra.")));
    contentLayout->addWidget(makeHelpRow(content, QStringLiteral("📐"), QStringLiteral("Analyse"),
        QStringLiteral("Comptage (un clic = un marqueur numéroté), mesures de distances en µm/mm (cliquer-"
                        "glisser, selon l'étalonnage de l'échelle) et quadrillage de comptage — sur l'image en "
                        "direct ou sur n'importe quelle image enregistrée ; l'image annotée peut être "
                        "sauvegardée.")));
    if (settings.featureLearningAidsEnabled()) {
        contentLayout->addWidget(makeHelpRow(content, QStringLiteral("🔬"), QStringLiteral("Nom du microscope"),
            QStringLiteral("Clique sur le titre en haut de l'écran pour voir les caractéristiques du microscope "
                            "et de la caméra, le schéma des parties du microscope et le glossaire. Le nom "
                            "affiché se règle dans les réglages.")));
    } else {
        contentLayout->addWidget(makeHelpRow(content, QStringLiteral("🔬"), QStringLiteral("Nom du microscope"),
            QStringLiteral("Clique sur le titre en haut de l'écran pour voir les caractéristiques du microscope "
                            "et de la caméra. Le nom affiché se règle dans les réglages.")));
    }
    if (settings.featureClassesEnabled()) {
        contentLayout->addWidget(makeHelpRow(content, QStringLiteral("👤"), QStringLiteral("Se connecter"),
            QStringLiteral("Choisis ta classe et ton groupe pour retrouver ta propre galerie de photos et vidéos.")));
    }
    if (settings.featurePinLockEnabled()) {
        contentLayout->addWidget(makeHelpRow(content, QStringLiteral("⚙"), QStringLiteral("Mode professeur"),
            QStringLiteral("Réservé à l'enseignant (protégé par un code PIN) : réglages avancés de la caméra, "
                            "gestion des classes et groupes, sauvegardes, choix de l'édition du logiciel, etc.")));
    } else {
        contentLayout->addWidget(makeHelpRow(content, QStringLiteral("⚙"), QStringLiteral("Réglages avancés"),
            QStringLiteral("Exposition, gain, balance des blancs, résolution, dossier d'enregistrement, "
                            "sauvegarde automatique, choix de l'édition du logiciel, etc.")));
    }
    contentLayout->addStretch();

    auto *closeButton = new QPushButton(QStringLiteral("Fermer"), this);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    root->addWidget(closeButton);
}
