#include "TrashDialog.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

#include "core/GalleryModel.h"

namespace {
const QStringList kVideoFilters = {"*.mp4", "*.avi"};
}

TrashDialog::TrashDialog(const QString &captureFolder, QWidget *parent)
    : QDialog(parent), m_captureFolder(captureFolder)
{
    setWindowTitle(QStringLiteral("Corbeille"));
    resize(640, 440);

    m_listWidget = new QListWidget(this);
    m_listWidget->setViewMode(QListView::IconMode);
    m_listWidget->setIconSize(QSize(140, 105));
    m_listWidget->setResizeMode(QListView::Adjust);
    m_listWidget->setSpacing(12);
    m_listWidget->setMovement(QListView::Static);

    auto *infoLabel = new QLabel(
        QStringLiteral("Les photos et vidéos supprimées restent ici 7 jours avant d'être effacées pour de bon."),
        this);
    infoLabel->setWordWrap(true);

    m_restoreButton = new QPushButton(QStringLiteral("Restaurer"), this);
    m_deleteButton = new QPushButton(QStringLiteral("Supprimer définitivement"), this);
    auto *closeButton = new QPushButton(QStringLiteral("Fermer"), this);
    m_restoreButton->setEnabled(false);
    m_deleteButton->setEnabled(false);

    auto *buttonRow = new QHBoxLayout();
    buttonRow->addWidget(m_restoreButton);
    buttonRow->addWidget(m_deleteButton);
    buttonRow->addStretch();
    buttonRow->addWidget(closeButton);

    auto *root = new QVBoxLayout(this);
    root->addWidget(infoLabel);
    root->addWidget(m_listWidget, 1);
    root->addLayout(buttonRow);

    connect(m_listWidget, &QListWidget::currentItemChanged, this, [this]() {
        const bool hasSelection = m_listWidget->currentItem() != nullptr;
        m_restoreButton->setEnabled(hasSelection);
        m_deleteButton->setEnabled(hasSelection);
    });
    connect(m_restoreButton, &QPushButton::clicked, this, &TrashDialog::onRestoreSelected);
    connect(m_deleteButton, &QPushButton::clicked, this, &TrashDialog::onDeleteForeverSelected);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);

    reloadItems();
}

void TrashDialog::reloadItems()
{
    m_listWidget->clear();

    const QDir trashDir(QDir(m_captureFolder).filePath(GalleryModel::trashFolderName()));
    const QStringList allFilters = QStringList{"*.png", "*.jpg", "*.jpeg"} + kVideoFilters;
    const QFileInfoList files = trashDir.entryInfoList(allFilters, QDir::Files, QDir::Time);

    for (const QFileInfo &fi : files) {
        auto *item = new QListWidgetItem(fi.fileName());
        item->setData(Qt::UserRole, fi.absoluteFilePath());
        const bool isVideo = kVideoFilters.contains("*." + fi.suffix().toLower());
        if (isVideo) {
            item->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
        } else {
            const QIcon icon(fi.absoluteFilePath());
            item->setIcon(icon.isNull() ? style()->standardIcon(QStyle::SP_FileIcon) : icon);
        }
        m_listWidget->addItem(item);
    }

    m_restoreButton->setEnabled(false);
    m_deleteButton->setEnabled(false);
}

void TrashDialog::onRestoreSelected()
{
    QListWidgetItem *item = m_listWidget->currentItem();
    if (!item)
        return;

    const QString path = item->data(Qt::UserRole).toString();
    const QFileInfo fi(path);
    QDir destDir(m_captureFolder);
    QString destPath = destDir.filePath(fi.fileName());
    int counter = 1;
    while (QFile::exists(destPath))
        destPath = destDir.filePath(QStringLiteral("%1_%2.%3").arg(fi.completeBaseName()).arg(counter++).arg(fi.suffix()));

    if (!QFile::rename(path, destPath)) {
        QMessageBox::warning(this, QStringLiteral("Restaurer"), QStringLiteral("Impossible de restaurer ce fichier."));
        return;
    }
    reloadItems();
}

void TrashDialog::onDeleteForeverSelected()
{
    QListWidgetItem *item = m_listWidget->currentItem();
    if (!item)
        return;

    const QString path = item->data(Qt::UserRole).toString();
    const auto answer = QMessageBox::question(this, QStringLiteral("Supprimer définitivement"),
                                               QStringLiteral("Supprimer définitivement %1 ? Cette action est irréversible.")
                                                   .arg(QFileInfo(path).fileName()));
    if (answer != QMessageBox::Yes)
        return;

    if (!QFile::remove(path)) {
        QMessageBox::warning(this, QStringLiteral("Supprimer"), QStringLiteral("Échec de la suppression du fichier."));
        return;
    }
    reloadItems();
}
