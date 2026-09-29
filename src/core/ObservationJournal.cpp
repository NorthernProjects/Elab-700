#include "ObservationJournal.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace {
const QString kJournalFileName = QStringLiteral("journal.json");
}

QVector<JournalEntry> ObservationJournal::load(const QString &folder)
{
    QVector<JournalEntry> entries;

    QFile file(QDir(folder).filePath(kJournalFileName));
    if (!file.open(QIODevice::ReadOnly))
        return entries;

    const QJsonArray array = QJsonDocument::fromJson(file.readAll()).array();
    entries.reserve(array.size());
    for (const QJsonValue &value : array) {
        const QJsonObject obj = value.toObject();
        JournalEntry entry;
        entry.dateTime = QDateTime::fromString(obj.value(QStringLiteral("date")).toString(), Qt::ISODate);
        entry.text = obj.value(QStringLiteral("text")).toString();
        entries.append(entry);
    }
    return entries;
}

void ObservationJournal::save(const QString &folder, const QVector<JournalEntry> &entries)
{
    QDir().mkpath(folder);

    QJsonArray array;
    for (const JournalEntry &entry : entries) {
        QJsonObject obj;
        obj.insert(QStringLiteral("date"), entry.dateTime.toString(Qt::ISODate));
        obj.insert(QStringLiteral("text"), entry.text);
        array.append(obj);
    }

    QFile file(QDir(folder).filePath(kJournalFileName));
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        file.write(QJsonDocument(array).toJson(QJsonDocument::Compact));
}
