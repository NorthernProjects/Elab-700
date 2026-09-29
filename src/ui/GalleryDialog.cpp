#include "GalleryDialog.h"

#include <algorithm>

#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QListWidget>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QPushButton>
#include <QStyle>
#include <QUrl>
#include <QVBoxLayout>

#include "ComparisonDialog.h"
#include "JournalDialog.h"
#include "PhotoEditorDialog.h"
#include "TrashDialog.h"
#include "core/FileUtils.h"
#include "core/GalleryModel.h"
#include "core/MailSender.h"
#include "core/ObservationJournal.h"

namespace {
constexpr int kTrashMaxAgeDays = 7;
}

GalleryDialog::GalleryDialog(GalleryModel *model, AppSettings *settings, QWidget *parent)
    : QDialog(parent), m_model(model), m_settings(settings)
{
    setWindowTitle(tr("Galerie"));
    resize(720, 480);

    m_listWidget = new QListWidget(this);
    m_listWidget->setViewMode(QListView::IconMode);
    m_listWidget->setIconSize(QSize(160, 120));
    m_listWidget->setResizeMode(QListView::Adjust);
    m_listWidget->setSpacing(12);
    m_listWidget->setMovement(QListView::Static);

    auto *openButton = new QPushButton(tr("Ouvrir"), this);
    auto *deleteButton = new QPushButton(tr("Supprimer"), this);
    m_emailButton = new QPushButton(tr("Envoyer par courriel..."), this);
    m_emailButton->setEnabled(false);
    m_editButton = new QPushButton(tr("Modifier..."), this);
    m_editButton->setEnabled(false);
    auto *compareButton = new QPushButton(tr("Comparer..."), this);
    auto *journalButton = new QPushButton(tr("Journal d'observations..."), this);
    auto *exportPdfButton = new QPushButton(tr("Exporter en PDF..."), this);
    auto *trashButton = new QPushButton(tr("Corbeille..."), this);
    auto *exportFolderButton = new QPushButton(tr("Exporter le dossier..."), this);
    auto *closeButton = new QPushButton(tr("Fermer"), this);

    auto *buttonRow = new QHBoxLayout();
    buttonRow->addWidget(openButton);
    buttonRow->addWidget(deleteButton);
    buttonRow->addWidget(m_editButton);
    buttonRow->addWidget(m_emailButton);
    buttonRow->addWidget(compareButton);
    buttonRow->addWidget(journalButton);
    buttonRow->addWidget(exportPdfButton);
    buttonRow->addWidget(trashButton);
    buttonRow->addWidget(exportFolderButton);
    buttonRow->addStretch();
    buttonRow->addWidget(closeButton);

    auto *root = new QVBoxLayout(this);
    root->addWidget(m_listWidget, 1);
    root->addLayout(buttonRow);

    connect(openButton, &QPushButton::clicked, this, &GalleryDialog::onOpenSelected);
    connect(deleteButton, &QPushButton::clicked, this, &GalleryDialog::onDeleteSelected);
    connect(m_emailButton, &QPushButton::clicked, this, &GalleryDialog::onEmailSelected);
    connect(m_editButton, &QPushButton::clicked, this, &GalleryDialog::onEditSelected);
    connect(compareButton, &QPushButton::clicked, this, &GalleryDialog::onCompareRequested);
    connect(journalButton, &QPushButton::clicked, this, &GalleryDialog::onJournalRequested);
    connect(exportPdfButton, &QPushButton::clicked, this, &GalleryDialog::onExportPdfRequested);
    connect(trashButton, &QPushButton::clicked, this, &GalleryDialog::onTrashRequested);
    connect(exportFolderButton, &QPushButton::clicked, this, &GalleryDialog::onExportFolderRequested);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_listWidget, &QListWidget::itemDoubleClicked, this, &GalleryDialog::onOpenSelected);
    connect(m_listWidget, &QListWidget::currentItemChanged, this, &GalleryDialog::onSelectionChanged);
    connect(m_model, &GalleryModel::modelReset, this, &GalleryDialog::reloadItems);

    GalleryModel::purgeOldTrash(m_model->folder(), kTrashMaxAgeDays);
    reloadItems();
}

void GalleryDialog::reloadItems()
{
    m_listWidget->clear();

    for (int row = 0; row < m_model->rowCount(); ++row) {
        const GalleryModel::Entry entry = m_model->entryAt(row);
        const QFileInfo fi(entry.filePath);

        auto *item = new QListWidgetItem(fi.fileName());
        item->setData(Qt::UserRole, entry.filePath);
        item->setData(Qt::UserRole + 1, entry.isVideo);

        if (entry.isVideo) {
            item->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
        } else {
            const QIcon icon(entry.filePath);
            item->setIcon(icon.isNull() ? style()->standardIcon(QStyle::SP_FileIcon) : icon);
        }

        m_listWidget->addItem(item);
    }

    onSelectionChanged();
}

void GalleryDialog::onSelectionChanged()
{
    QListWidgetItem *item = m_listWidget->currentItem();
    const bool isPhoto = item && !item->data(Qt::UserRole + 1).toBool();
    if (m_settings->featureClassesEnabled()) {
        // School flow: sends to the class teacher's configured address.
        m_emailButton->setEnabled(isPhoto && !m_settings->currentTeacherEmail().isEmpty());
    } else {
        // No roster: onEmailSelected() asks for a recipient at send time, so
        // any photo is eligible.
        m_emailButton->setEnabled(isPhoto);
    }
    m_editButton->setEnabled(isPhoto);
}

void GalleryDialog::onOpenSelected()
{
    QListWidgetItem *item = m_listWidget->currentItem();
    if (!item)
        return;

    const QString path = item->data(Qt::UserRole).toString();
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void GalleryDialog::onDeleteSelected()
{
    QListWidgetItem *item = m_listWidget->currentItem();
    if (!item)
        return;

    const int row = m_listWidget->row(item);
    const QString path = item->data(Qt::UserRole).toString();

    const auto answer = QMessageBox::question(this, tr("Supprimer"),
                                               tr("Mettre %1 à la corbeille ?")
                                                   .arg(QFileInfo(path).fileName()));
    if (answer != QMessageBox::Yes)
        return;

    if (!m_model->trashFile(row)) {
        QMessageBox::warning(this, tr("Supprimer"), tr("Échec de la suppression du fichier."));
        return;
    }
    reloadItems();
}

void GalleryDialog::onEmailSelected()
{
    QListWidgetItem *item = m_listWidget->currentItem();
    if (!item)
        return;

    const QString path = item->data(Qt::UserRole).toString();

    QString recipientName;
    QString recipientEmail;
    QString subject;
    if (m_settings->featureClassesEnabled()) {
        recipientName = m_settings->currentTeacherName();
        recipientEmail = m_settings->currentTeacherEmail();
        if (recipientEmail.isEmpty()) {
            QMessageBox::warning(this, tr("Envoyer par courriel"),
                                  tr("Aucun courriel d'enseignant configuré pour cette classe."));
            return;
        }
        subject = tr("Photo microscope — %1 / %2")
            .arg(m_settings->currentClassName(), m_settings->currentGroupName());
    } else {
        // No pre-configured "class teacher" to send to — just ask who, each
        // time (nothing persisted, no recipient list to maintain).
        bool ok = false;
        recipientEmail = QInputDialog::getText(this, tr("Envoyer par courriel"),
            tr("Adresse courriel du destinataire :"), QLineEdit::Normal, QString(), &ok);
        if (!ok || recipientEmail.isEmpty())
            return;
        recipientName = recipientEmail;
        subject = tr("Photo microscope — E-Lab 700");
    }

    QString error;
    if (!MailSender::sendWithAttachment(recipientName, recipientEmail, subject, QString(), path, &error))
        QMessageBox::warning(this, tr("Envoyer par courriel"), error);
}

void GalleryDialog::onEditSelected()
{
    QListWidgetItem *item = m_listWidget->currentItem();
    if (!item || item->data(Qt::UserRole + 1).toBool())
        return;

    const QString path = item->data(Qt::UserRole).toString();
    PhotoEditorDialog dialog(path, this);
    dialog.exec();
    reloadItems();
}

void GalleryDialog::onCompareRequested()
{
    ComparisonDialog dialog(m_model, this);
    dialog.exec();
}

void GalleryDialog::onJournalRequested()
{
    const QString title = m_settings->currentClassName().isEmpty()
        ? tr("Journal d'observations")
        : QStringLiteral("%1 — %2").arg(m_settings->currentClassName(), m_settings->currentGroupName());
    JournalDialog dialog(m_model->folder(), title, this);
    dialog.exec();
}

void GalleryDialog::onExportPdfRequested()
{
    QStringList photoPaths;
    for (int row = 0; row < m_model->rowCount(); ++row) {
        const GalleryModel::Entry entry = m_model->entryAt(row);
        if (!entry.isVideo)
            photoPaths.append(entry.filePath);
    }
    if (photoPaths.isEmpty() && ObservationJournal::load(m_model->folder()).isEmpty()) {
        QMessageBox::information(this, tr("Exporter en PDF"),
                                  tr("Aucune photo ni note de journal à exporter dans cette galerie."));
        return;
    }

    const QString defaultName = QStringLiteral("%1_%2.pdf")
        .arg(m_settings->currentClassName().isEmpty() ? tr("galerie") : m_settings->currentClassName())
        .arg(m_settings->currentGroupName());
    const QString outputPath = QFileDialog::getSaveFileName(this, tr("Exporter en PDF"),
                                                              defaultName, tr("PDF (*.pdf)"));
    if (outputPath.isEmpty())
        return;

    QVector<JournalEntry> journalEntries = ObservationJournal::load(m_model->folder());
    std::sort(journalEntries.begin(), journalEntries.end(), [](const JournalEntry &a, const JournalEntry &b) {
        return a.dateTime < b.dateTime; // chronological, like reading a logbook front to back
    });

    QPdfWriter writer(outputPath);
    writer.setPageSize(QPageSize(QPageSize::A4));
    writer.setResolution(150);
    QPainter painter(&writer);

    const QString title = QStringLiteral("%1 — %2")
        .arg(m_settings->currentClassName(), m_settings->currentGroupName());

    bool firstPageEmitted = false;
    auto beginPage = [&]() {
        if (firstPageEmitted)
            writer.newPage();
        firstPageEmitted = true;
    };

    if (!journalEntries.isEmpty()) {
        const QRect pageRect(0, 0, writer.width(), writer.height());
        QFont journalTitleFont = painter.font();
        journalTitleFont.setPointSize(14);
        journalTitleFont.setBold(true);
        QFont entryDateFont = painter.font();
        entryDateFont.setPointSize(10);
        entryDateFont.setBold(true);
        QFont entryTextFont = painter.font();
        entryTextFont.setPointSize(10);
        entryTextFont.setBold(false);

        int cursorY = 0;
        auto startJournalPage = [&]() {
            beginPage();
            painter.setFont(journalTitleFont);
            painter.drawText(QRect(pageRect.left(), pageRect.top(), pageRect.width(), 60),
                              Qt::AlignLeft | Qt::AlignVCenter,
                              tr("%1 — Journal d'observations").arg(title));
            cursorY = pageRect.top() + 80;
        };
        startJournalPage();

        for (const JournalEntry &entry : journalEntries) {
            const QString dateStr = entry.dateTime.toString(QStringLiteral("dd/MM/yyyy HH:mm"));
            painter.setFont(entryTextFont);
            const QRect measureRect(pageRect.left(), 0, pageRect.width(), pageRect.height());
            const int textHeight = painter.boundingRect(measureRect, Qt::TextWordWrap, entry.text).height();
            const int neededHeight = 30 + textHeight + 20; // date line + wrapped text + gap

            if (cursorY + neededHeight > pageRect.bottom() - 40)
                startJournalPage();

            painter.setFont(entryDateFont);
            painter.drawText(QRect(pageRect.left(), cursorY, pageRect.width(), 30), Qt::AlignLeft, dateStr);
            cursorY += 30;

            painter.setFont(entryTextFont);
            const QRect textRect(pageRect.left(), cursorY, pageRect.width(), pageRect.bottom() - cursorY);
            painter.drawText(textRect, Qt::AlignLeft | Qt::TextWordWrap, entry.text);
            cursorY += textHeight + 20;
        }
    }

    for (int i = 0; i < photoPaths.size(); ++i) {
        beginPage();

        const QRect pageRect(0, 0, writer.width(), writer.height());
        QFont titleFont = painter.font();
        titleFont.setPointSize(14);
        titleFont.setBold(true);
        painter.setFont(titleFont);
        painter.drawText(QRect(pageRect.left(), pageRect.top(), pageRect.width(), 60),
                          Qt::AlignLeft | Qt::AlignVCenter, title);

        const QPixmap photo(photoPaths.at(i));
        if (!photo.isNull()) {
            const QRect imageArea(pageRect.left(), pageRect.top() + 80, pageRect.width(), pageRect.height() - 160);
            const QSize scaled = photo.size().scaled(imageArea.size(), Qt::KeepAspectRatio);
            const QRect destRect(
                imageArea.left() + (imageArea.width() - scaled.width()) / 2,
                imageArea.top() + (imageArea.height() - scaled.height()) / 2,
                scaled.width(), scaled.height());
            painter.drawPixmap(destRect, photo);
        }

        QFont captionFont = painter.font();
        captionFont.setPointSize(10);
        captionFont.setBold(false);
        painter.setFont(captionFont);
        painter.drawText(QRect(pageRect.left(), pageRect.bottom() - 60, pageRect.width(), 40),
                          Qt::AlignLeft | Qt::AlignVCenter, QFileInfo(photoPaths.at(i)).fileName());
    }

    painter.end();
    QMessageBox::information(this, tr("Exporter en PDF"),
                              tr("PDF enregistré : %1").arg(outputPath));
}

void GalleryDialog::onTrashRequested()
{
    TrashDialog dialog(m_model->folder(), this);
    dialog.exec();
    m_model->refresh();
}

void GalleryDialog::onExportFolderRequested()
{
    const QString destinationRoot = QFileDialog::getExistingDirectory(
        this, tr("Choisir où exporter (clé USB, autre dossier...)"));
    if (destinationRoot.isEmpty())
        return;

    const QString className = m_settings->currentClassName().isEmpty()
        ? tr("galerie") : m_settings->currentClassName();
    const QString groupName = m_settings->currentGroupName();
    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
    const QString folderName = groupName.isEmpty()
        ? QStringLiteral("%1_%2").arg(className, timestamp)
        : QStringLiteral("%1_%2_%3").arg(className, groupName, timestamp);
    const QString destination = QDir(destinationRoot).filePath(folderName);

    // The Corbeille subfolder holds items the group already chose to
    // delete — a backup shouldn't resurrect them.
    if (!FileUtils::copyFolderRecursively(m_model->folder(), destination, GalleryModel::trashFolderName())) {
        QMessageBox::warning(this, tr("Exporter le dossier"),
                              tr("L'export a échoué ou est incomplet. Vérifiez l'espace disponible."));
        return;
    }

    QMessageBox::information(this, tr("Exporter le dossier"),
                              tr("Dossier exporté avec succès :\n%1").arg(destination));
}
