#include "JournalDialog.h"

#include <algorithm>

#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include "core/ObservationJournal.h"

JournalDialog::JournalDialog(const QString &folder, const QString &title, QWidget *parent)
    : QDialog(parent), m_folder(folder)
{
    setWindowTitle(tr("Journal d'observations"));
    resize(560, 520);

    auto *root = new QVBoxLayout(this);

    auto *titleLabel = new QLabel(title.isEmpty() ? tr("Journal d'observations") : title, this);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleFont.setPointSize(titleFont.pointSize() + 2);
    titleLabel->setFont(titleFont);
    root->addWidget(titleLabel);

    m_listWidget = new QListWidget(this);
    root->addWidget(m_listWidget, 1);

    m_deleteButton = new QPushButton(tr("Supprimer l'entrée sélectionnée"), this);
    m_deleteButton->setEnabled(false);
    root->addWidget(m_deleteButton);

    auto *newEntryLabel = new QLabel(tr("Nouvelle observation :"), this);
    root->addWidget(newEntryLabel);

    m_newEntryEdit = new QPlainTextEdit(this);
    m_newEntryEdit->setPlaceholderText(tr("Ce que le groupe observe aujourd'hui..."));
    m_newEntryEdit->setFixedHeight(80);
    root->addWidget(m_newEntryEdit);

    auto *addButton = new QPushButton(tr("Ajouter au journal"), this);
    auto *closeButton = new QPushButton(tr("Fermer"), this);
    auto *buttonRow = new QHBoxLayout();
    buttonRow->addWidget(addButton);
    buttonRow->addStretch();
    buttonRow->addWidget(closeButton);
    root->addLayout(buttonRow);

    connect(addButton, &QPushButton::clicked, this, &JournalDialog::onAddEntry);
    connect(m_deleteButton, &QPushButton::clicked, this, &JournalDialog::onDeleteSelected);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_listWidget, &QListWidget::currentRowChanged, this, [this](int row) {
        m_deleteButton->setEnabled(row >= 0);
    });

    reloadEntries();
}

void JournalDialog::reloadEntries()
{
    m_listWidget->clear();

    m_entries = ObservationJournal::load(m_folder);
    // Newest first: the most useful view when quickly checking "what did we
    // already note today" before adding another entry.
    std::sort(m_entries.begin(), m_entries.end(), [](const JournalEntry &a, const JournalEntry &b) {
        return a.dateTime > b.dateTime;
    });

    for (const JournalEntry &entry : m_entries) {
        m_listWidget->addItem(
            QStringLiteral("%1\n%2").arg(entry.dateTime.toString(QStringLiteral("dd/MM/yyyy HH:mm")), entry.text));
    }
}

void JournalDialog::onAddEntry()
{
    const QString text = m_newEntryEdit->toPlainText().trimmed();
    if (text.isEmpty())
        return;

    QVector<JournalEntry> entries = ObservationJournal::load(m_folder);
    JournalEntry entry;
    entry.dateTime = QDateTime::currentDateTime();
    entry.text = text;
    entries.append(entry);
    ObservationJournal::save(m_folder, entries);

    m_newEntryEdit->clear();
    reloadEntries();
}

void JournalDialog::onDeleteSelected()
{
    const int row = m_listWidget->currentRow();
    if (row < 0 || row >= m_entries.size())
        return;

    if (QMessageBox::question(this, tr("Supprimer"),
                               tr("Supprimer cette entrée du journal ?"))
        != QMessageBox::Yes)
        return;

    m_entries.remove(row);
    ObservationJournal::save(m_folder, m_entries);
    reloadEntries();
}
