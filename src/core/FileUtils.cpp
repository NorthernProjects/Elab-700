#include "FileUtils.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace FileUtils {

bool copyFolderRecursively(const QString &sourceDir, const QString &destDir, const QString &excludeSubfolderName)
{
    QDir source(sourceDir);
    if (!source.exists())
        return false;
    if (!QDir().mkpath(destDir))
        return false;

    const QFileInfoList entries = source.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries);
    for (const QFileInfo &entry : entries) {
        if (entry.isDir() && entry.fileName() == excludeSubfolderName)
            continue;
        const QString destPath = QDir(destDir).filePath(entry.fileName());
        if (entry.isDir()) {
            if (!copyFolderRecursively(entry.absoluteFilePath(), destPath, excludeSubfolderName))
                return false;
        } else if (!QFile::copy(entry.absoluteFilePath(), destPath)) {
            return false;
        }
    }
    return true;
}

} // namespace FileUtils
