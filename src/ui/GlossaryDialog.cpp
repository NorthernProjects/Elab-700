#include "GlossaryDialog.h"

#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QVector>

#include "SmoothScrollArea.h"

namespace {

// const char* (not QString): each pair is registered for translation via
// QT_TRANSLATE_NOOP under the "GlossaryDialog" context (matching the class
// below) since this table is built at static-init time, outside any
// QObject — tr() itself can't run here. The constructor below runs the
// actual tr(term.word)/tr(term.definition) lookup once it has a real
// GlossaryDialog `this` to call it on.
struct GlossaryTerm {
    const char *word;
    const char *definition;
};

// Vocabulary a student actually encounters while using the microscope and
// looking at a slide — equipment terms first, then the basic biology words
// they'll need to describe what they see.
const QVector<GlossaryTerm> kTerms = {
    {QT_TRANSLATE_NOOP("GlossaryDialog", "Lame"),
     QT_TRANSLATE_NOOP("GlossaryDialog", "Petite plaque de verre transparente sur laquelle on dépose l'échantillon à observer.")},
    {QT_TRANSLATE_NOOP("GlossaryDialog", "Lamelle"),
     QT_TRANSLATE_NOOP("GlossaryDialog", "Très fine plaque de verre carrée qu'on pose par-dessus l'échantillon, sur la lame, "
                     "pour l'aplatir et le protéger.")},
    {QT_TRANSLATE_NOOP("GlossaryDialog", "Préparation microscopique"),
     QT_TRANSLATE_NOOP("GlossaryDialog", "L'ensemble lame + échantillon + lamelle, prêt à être observé au microscope.")},
    {QT_TRANSLATE_NOOP("GlossaryDialog", "Objectif"),
     QT_TRANSLATE_NOOP("GlossaryDialog", "Lentille placée juste au-dessus de l'échantillon. Le microscope en a plusieurs "
                     "(x4, x10, x40...) montés sur la tourelle.")},
    {QT_TRANSLATE_NOOP("GlossaryDialog", "Oculaire"),
     QT_TRANSLATE_NOOP("GlossaryDialog", "Lentille dans laquelle on regarde, en haut du microscope, généralement x10.")},
    {QT_TRANSLATE_NOOP("GlossaryDialog", "Grossissement"),
     QT_TRANSLATE_NOOP("GlossaryDialog", "Nombre de fois où l'image est agrandie. Il se calcule en multipliant le grossissement "
                     "de l'oculaire par celui de l'objectif (par exemple 10 x 40 = grossissement x400).")},
    {QT_TRANSLATE_NOOP("GlossaryDialog", "Tourelle porte-objectifs"),
     QT_TRANSLATE_NOOP("GlossaryDialog", "Pièce qui tourne pour changer d'objectif sans démonter le microscope.")},
    {QT_TRANSLATE_NOOP("GlossaryDialog", "Platine"),
     QT_TRANSLATE_NOOP("GlossaryDialog", "Plateau sur lequel on pose la lame, avec un trou au milieu pour laisser passer la lumière.")},
    {QT_TRANSLATE_NOOP("GlossaryDialog", "Valets (pinces de la platine)"),
     QT_TRANSLATE_NOOP("GlossaryDialog", "Petites pinces métalliques qui maintiennent la lame en place sur la platine.")},
    {QT_TRANSLATE_NOOP("GlossaryDialog", "Vis macrométrique"),
     QT_TRANSLATE_NOOP("GlossaryDialog", "Grosse molette qui fait la mise au point rapide, en bougeant beaucoup la platine.")},
    {QT_TRANSLATE_NOOP("GlossaryDialog", "Vis micrométrique"),
     QT_TRANSLATE_NOOP("GlossaryDialog", "Petite molette qui fait la mise au point fine et précise, pour obtenir une image bien nette.")},
    {QT_TRANSLATE_NOOP("GlossaryDialog", "Condenseur"),
     QT_TRANSLATE_NOOP("GlossaryDialog", "Pièce sous la platine qui concentre la lumière vers l'échantillon.")},
    {QT_TRANSLATE_NOOP("GlossaryDialog", "Diaphragme"),
     QT_TRANSLATE_NOOP("GlossaryDialog", "Réglage qui contrôle la quantité de lumière qui traverse l'échantillon.")},
    {QT_TRANSLATE_NOOP("GlossaryDialog", "Source lumineuse"),
     QT_TRANSLATE_NOOP("GlossaryDialog", "Lampe intégrée à la base du microscope, qui éclaire l'échantillon par en dessous.")},
    {QT_TRANSLATE_NOOP("GlossaryDialog", "Tête trinoculaire"),
     QT_TRANSLATE_NOOP("GlossaryDialog", "Partie du haut du microscope qui porte les oculaires et un troisième tube pour "
                     "brancher une caméra.")},
    {QT_TRANSLATE_NOOP("GlossaryDialog", "Bras"),
     QT_TRANSLATE_NOOP("GlossaryDialog", "Partie qui relie le socle à la tête du microscope. C'est par là qu'on le porte.")},
    {QT_TRANSLATE_NOOP("GlossaryDialog", "Socle (base)"),
     QT_TRANSLATE_NOOP("GlossaryDialog", "Partie du bas qui repose sur la table et supporte tout le microscope.")},
    {QT_TRANSLATE_NOOP("GlossaryDialog", "Mise au point"),
     QT_TRANSLATE_NOOP("GlossaryDialog", "Réglage de la netteté de l'image, à l'aide des vis macrométrique et micrométrique.")},
    {QT_TRANSLATE_NOOP("GlossaryDialog", "Champ de vision"),
     QT_TRANSLATE_NOOP("GlossaryDialog", "Zone que l'on voit à travers le microscope, à un grossissement donné.")},
    {QT_TRANSLATE_NOOP("GlossaryDialog", "Immersion (huile à immersion)"),
     QT_TRANSLATE_NOOP("GlossaryDialog", "Liquide spécial utilisé avec certains objectifs très puissants (x100) pour mieux "
                     "faire passer la lumière.")},
    {QT_TRANSLATE_NOOP("GlossaryDialog", "Cellule"),
     QT_TRANSLATE_NOOP("GlossaryDialog", "Plus petite unité vivante qui compose les êtres vivants. On peut souvent la voir "
                     "au microscope.")},
    {QT_TRANSLATE_NOOP("GlossaryDialog", "Noyau"),
     QT_TRANSLATE_NOOP("GlossaryDialog", "Partie de la cellule qui contient l'ADN, souvent visible comme un point plus foncé.")},
    {QT_TRANSLATE_NOOP("GlossaryDialog", "Membrane"),
     QT_TRANSLATE_NOOP("GlossaryDialog", "Fine enveloppe qui entoure la cellule et la protège.")},
    {QT_TRANSLATE_NOOP("GlossaryDialog", "Cytoplasme"),
     QT_TRANSLATE_NOOP("GlossaryDialog", "Substance qui remplit l'intérieur de la cellule, tout autour du noyau.")},
    {QT_TRANSLATE_NOOP("GlossaryDialog", "Coloration"),
     QT_TRANSLATE_NOOP("GlossaryDialog", "Technique qui consiste à ajouter un colorant à l'échantillon pour mieux voir certaines "
                     "structures au microscope.")},
    {QT_TRANSLATE_NOOP("GlossaryDialog", "Résolution (pouvoir de résolution)"),
     QT_TRANSLATE_NOOP("GlossaryDialog", "Capacité du microscope à distinguer deux détails très proches l'un de l'autre.")},
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
    setWindowTitle(tr("Glossaire de microscopie"));
    resize(560, 620);

    auto *root = new QVBoxLayout(this);

    auto *searchEdit = new QLineEdit(this);
    searchEdit->setPlaceholderText(tr("Rechercher un mot (ex : lamelle, oculaire...)"));
    searchEdit->setClearButtonEnabled(true);
    root->addWidget(searchEdit);

    auto *scrollArea = new SmoothScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    root->addWidget(scrollArea, 1);

    auto *content = new QWidget(scrollArea);
    scrollArea->setWidget(content);
    auto *contentLayout = new QVBoxLayout(content);

    auto *noResultsLabel = new QLabel(tr("Aucun mot ne correspond à cette recherche."), content);
    noResultsLabel->setWordWrap(true);
    noResultsLabel->hide();
    contentLayout->addWidget(noResultsLabel);

    QVector<QWidget *> rows;
    rows.reserve(kTerms.size());
    for (const GlossaryTerm &term : kTerms) {
        auto *row = makeGlossaryRow(content, tr(term.word), tr(term.definition));
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

    auto *closeButton = new QPushButton(tr("Fermer"), this);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    root->addWidget(closeButton);
}
