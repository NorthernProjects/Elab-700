#pragma once

#include <QDialog>
#include <QVector>

#include "core/ObservationJournal.h"

class QListWidget;
class QPlainTextEdit;
class QPushButton;

// Carnet de bord: a simple dated logbook for one class/group's capture
// folder (see ObservationJournal). Reachable from the gallery's "Journal
// d'observations..." button — entries persist in that folder's journal.json
// so they travel with the group's photos/videos.
class JournalDialog : public QDialog {
    Q_OBJECT

public:
    // title is shown in the dialog heading (e.g. the class/group name);
    // folder is where journal.json lives.
    JournalDialog(const QString &folder, const QString &title, QWidget *parent = nullptr);

private slots:
    void onAddEntry();
    void onDeleteSelected();

private:
    void reloadEntries();

    QString m_folder;
    // Sorted (newest first) view of what's currently shown in m_listWidget,
    // kept in lockstep with it so onDeleteSelected() can map a row straight
    // to an entry instead of matching timestamps back against disk (Qt::
    // ISODate round-trips lose sub-second precision, so two entries added
    // in the same second would otherwise be ambiguous).
    QVector<JournalEntry> m_entries;
    QListWidget *m_listWidget;
    QPlainTextEdit *m_newEntryEdit;
    QPushButton *m_deleteButton;
};
