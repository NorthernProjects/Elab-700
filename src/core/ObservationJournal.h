#pragma once

#include <QDateTime>
#include <QString>
#include <QVector>

// One dated note in a group's logbook ("carnet de bord").
struct JournalEntry {
    QDateTime dateTime;
    QString text;
};

// Persists a simple dated logbook as journal.json inside a capture folder
// (one per class/group — see AppSettings::groupCaptureFolder — or the
// shared capture folder when the classes feature is off). Kept as a thin
// static helper, not a QObject: callers (JournalDialog, the PDF export)
// just need load/save, nothing reactive.
class ObservationJournal {
public:
    static QVector<JournalEntry> load(const QString &folder);
    static void save(const QString &folder, const QVector<JournalEntry> &entries);
};
