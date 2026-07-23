#include "TeacherOverviewDialog.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "GalleryDialog.h"
#include "core/GalleryModel.h"

namespace {
constexpr int kClassRole = Qt::UserRole + 1;
constexpr int kGroupRole = Qt::UserRole + 2;
}

TeacherOverviewDialog::TeacherOverviewDialog(AppSettings *settings, QWidget *parent)
    : QDialog(parent), m_settings(settings)
{
    setWindowTitle(QStringLiteral("Vue d'ensemble des groupes"));
    resize(560, 520);

    m_tree = new QTreeWidget(this);
    m_tree->setColumnCount(2);
    m_tree->setHeaderLabels({QStringLiteral("Classe / Groupe"), QStringLiteral("Captures")});
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);

    auto *infoLabel = new QLabel(
        QStringLiteral("Double-clique sur un groupe pour ouvrir sa galerie."), this);

    auto *openButton = new QPushButton(QStringLiteral("Ouvrir la galerie"), this);
    auto *closeButton = new QPushButton(QStringLiteral("Fermer"), this);
    openButton->setEnabled(false);

    auto *buttonRow = new QHBoxLayout();
    buttonRow->addWidget(openButton);
    buttonRow->addStretch();
    buttonRow->addWidget(closeButton);

    auto *root = new QVBoxLayout(this);
    root->addWidget(infoLabel);
    root->addWidget(m_tree, 1);
    root->addLayout(buttonRow);

    connect(m_tree, &QTreeWidget::itemDoubleClicked, this, &TeacherOverviewDialog::onItemDoubleClicked);
    connect(m_tree, &QTreeWidget::currentItemChanged, this, [openButton](QTreeWidgetItem *item) {
        openButton->setEnabled(item && item->data(0, kGroupRole).isValid());
    });
    connect(openButton, &QPushButton::clicked, this, &TeacherOverviewDialog::onOpenSelectedGallery);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);

    reloadCounts();
}

void TeacherOverviewDialog::reloadCounts()
{
    m_tree->clear();

    for (const SchoolClass &schoolClass : m_settings->classes()) {
        auto *classItem = new QTreeWidgetItem(m_tree, {schoolClass.name, QString()});
        classItem->setData(0, kClassRole, schoolClass.name);
        QFont boldFont = classItem->font(0);
        boldFont.setBold(true);
        classItem->setFont(0, boldFont);

        for (const QString &groupName : schoolClass.groups) {
            const QString folder = m_settings->groupCaptureFolder(schoolClass.name, groupName);
            GalleryModel model;
            model.setFolder(folder);

            int photoCount = 0;
            int videoCount = 0;
            for (int row = 0; row < model.rowCount(); ++row) {
                if (model.entryAt(row).isVideo)
                    ++videoCount;
                else
                    ++photoCount;
            }

            const QString countText = QStringLiteral("%1 photo(s), %2 vidéo(s)").arg(photoCount).arg(videoCount);
            auto *groupItem = new QTreeWidgetItem(classItem, {groupName, countText});
            groupItem->setData(0, kClassRole, schoolClass.name);
            groupItem->setData(0, kGroupRole, groupName);
        }
        classItem->setExpanded(true);
    }
}

void TeacherOverviewDialog::onItemDoubleClicked(QTreeWidgetItem *item, int /*column*/)
{
    if (item && item->data(0, kGroupRole).isValid())
        onOpenSelectedGallery();
}

void TeacherOverviewDialog::onOpenSelectedGallery()
{
    QTreeWidgetItem *item = m_tree->currentItem();
    if (!item || !item->data(0, kGroupRole).isValid())
        return;

    const QString className = item->data(0, kClassRole).toString();
    const QString groupName = item->data(0, kGroupRole).toString();

    // Briefly switch the active group so the gallery's "envoyer par
    // courriel" button addresses the right teacher for THIS class, then
    // restore whatever was active before — the overview shouldn't leave a
    // side effect on the logged-in session.
    const QString previousClass = m_settings->currentClassName();
    const QString previousGroup = m_settings->currentGroupName();
    m_settings->setActiveGroup(className, groupName);

    GalleryModel model;
    model.setFolder(m_settings->groupCaptureFolder(className, groupName));
    GalleryDialog dialog(&model, m_settings, this);
    dialog.exec();

    m_settings->setActiveGroup(previousClass, previousGroup);
    reloadCounts();
}
