#include "GlossaryDialog.h"

#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QVector>

#include "SmoothScrollArea.h"

namespace {

struct GlossaryTerm {
    QString word;
    QString definition;
};

// Vocabulary a student actually encounters while using the microscope and
// looking at a slide — equipment terms first, then the basic biology words
// they'll need to describe what they see.
const QVector<GlossaryTerm> kTerms = {
    {QStringLiteral("Lame"),
     QStringLiteral("Petite plaque de verre transparente sur laquelle on dépose l'échantillon à observer.")},
    {QStringLiteral("Lamelle"),
     QStringLiteral("Très fine plaque de verre carrée qu'on pose par-dessus l'échantillon, sur la lame, "
                     "pour l'aplatir et le protéger.")},
    {QStringLiteral("Préparation microscopique"),
     QStringLiteral("L'ensemble lame + échantillon + lamelle, prêt à être observé au microscope.")},
    {QStringLiteral("Objectif"),
     QStringLiteral("Lentille placée juste au-dessus de l'échantillon. Le microscope en a plusieurs "
                     "(x4, x10, x40...) montés sur la tourelle.")},
    {QStringLiteral("Oculaire"),
     QStringLiteral("Lentille dans laquelle on regarde, en haut du microscope, généralement x10.")},
    {QStringLiteral("Grossissement"),
     QStringLiteral("Nombre de fois où l'image est agrandie. Il se calcule en multipliant le grossissement "
                     "de l'oculaire par celui de l'objectif (par exemple 10 x 40 = grossissement x400).")},
    {QStringLiteral("Tourelle porte-objectifs"),
     QStringLiteral("Pièce qui tourne pour changer d'objectif sans démonter le microscope.")},
    {QStringLiteral("Platine"),
     QStringLiteral("Plateau sur lequel on pose la lame, avec un trou au milieu pour laisser passer la lumière.")},
    {QStringLiteral("Valets (pinces de la platine)"),
     QStringLiteral("Petites pinces métalliques qui maintiennent la lame en place sur la platine.")},
    {QStringLiteral("Vis macrométrique"),
     QStringLiteral("Grosse molette qui fait la mise au point rapide, en bougeant beaucoup la platine.")},
    {QStringLiteral("Vis micrométrique"),
     QStringLiteral("Petite molette qui fait la mise au point fine et précise, pour obtenir une image bien nette.")},
    {QStringLiteral("Condenseur"),
     QStringLiteral("Pièce sous la platine qui concentre la lumière vers l'échantillon.")},
    {QStringLiteral("Diaphragme"),
     QStringLiteral("Réglage qui contrôle la quantité de lumière qui traverse l'échantillon.")},
    {QStringLiteral("Source lumineuse"),
     QStringLiteral("Lampe intégrée à la base du microscope, qui éclaire l'échantillon par en dessous.")},
    {QStringLiteral("Tête trinoculaire"),
     QStringLiteral("Partie du haut du microscope qui porte les oculaires et un troisième tube pour "
                     "brancher une caméra.")},
    {QStringLiteral("Bras"),
     QStringLiteral("Partie qui relie le socle à la tête du microscope. C'est par là qu'on le porte.")},
    {QStringLiteral("Socle (base)"),
     QStringLiteral("Partie du bas qui repose sur la table et supporte tout le microscope.")},
    {QStringLiteral("Mise au point"),
     QStringLiteral("Réglage de la netteté de l'image, à l'aide des vis macrométrique et micrométrique.")},
    {QStringLiteral("Champ de vision"),
     QStringLiteral("Zone que l'on voit à travers le microscope, à un grossissement donné.")},
    {QStringLiteral("Immersion (huile à immersion)"),
     QStringLiteral("Liquide spécial utilisé avec certains objectifs très puissants (x100) pour mieux "
                     "faire passer la lumière.")},
    {QStringLiteral("Cellule"),
     QStringLiteral("Plus petite unité vivante qui compose les êtres vivants. On peut souvent la voir "
                     "au microscope.")},
    {QStringLiteral("Noyau"),
     QStringLiteral("Partie de la cellule qui contient l'ADN, souvent visible comme un point plus foncé.")},
    {QStringLiteral("Membrane"),
     QStringLiteral("Fine enveloppe qui entoure la cellule et la protège.")},
    {QStringLiteral("Cytoplasme"),
     QStringLiteral("Substance qui remplit l'intérieur de la cellule, tout autour du noyau.")},
    {QStringLiteral("Coloration"),
     QStringLiteral("Technique qui consiste à ajouter un colorant à l'échantillon pour mieux voir certaines "
                     "structures au microscope.")},
    {QStringLiteral("Résolution (pouvoir de résolution)"),
     QStringLiteral("Capacité du microscope à distinguer deux détails très proches l'un de l'autre.")},
};

QWidget *makeGlossaryRow(QWidget *parent, const QString &word, const QString &definition)
{
    auto *row = new QWidget(parent);
    row->setProperty("glossarySearchText", (word + QStringLiteral(" ") + definition).toLower());

    auto *layout = new QVBoxLayout(row);
    layout->setContentsMargins(0, 8, 0, 8);
    layout->setSpacing(2);

    auto *wordLabel = new QLabel(QStringLiteral("<b>%1</b>").arg(word), row);
    auto *defLabel = new QLabel(definition, row);
    defLabel->setWordWrap(true);

    layout->addWidget(wordLabel);
    layout->addWidget(defLabel);
    return row;
}

} // namespace

GlossaryDialog::GlossaryDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Glossaire de microscopie"));
    resize(560, 620);

    auto *root = new QVBoxLayout(this);

    auto *searchEdit = new QLineEdit(this);
    searchEdit->setPlaceholderText(QStringLiteral("Rechercher un mot (ex : lamelle, oculaire...)"));
    searchEdit->setClearButtonEnabled(true);
    root->addWidget(searchEdit);

    auto *scrollArea = new SmoothScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    root->addWidget(scrollArea, 1);

    auto *content = new QWidget(scrollArea);
    scrollArea->setWidget(content);
    auto *contentLayout = new QVBoxLayout(content);

    auto *noResultsLabel = new QLabel(QStringLiteral("Aucun mot ne correspond à cette recherche."), content);
    noResultsLabel->setWordWrap(true);
    noResultsLabel->hide();
    contentLayout->addWidget(noResultsLabel);

    QVector<QWidget *> rows;
    rows.reserve(kTerms.size());
    for (const GlossaryTerm &term : kTerms) {
        auto *row = makeGlossaryRow(content, term.word, term.definition);
        contentLayout->addWidget(row);
        rows.append(row);
    }
    contentLayout->addStretch();

    connect(searchEdit, &QLineEdit::textChanged, this, [rows, noResultsLabel](const QString &text) {
        const QString needle = text.trimmed().toLower();
        bool anyVisible = false;
        for (QWidget *row : rows) {
            const bool matches = needle.isEmpty() || row->property("glossarySearchText").toString().contains(needle);
            row->setVisible(matches);
            anyVisible = anyVisible || matches;
        }
        noResultsLabel->setVisible(!anyVisible);
    });

    auto *closeButton = new QPushButton(QStringLiteral("Fermer"), this);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    root->addWidget(closeButton);
}
