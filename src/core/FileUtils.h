#pragma once

#include <QString>

// Small shared helpers used by both the manual "Exporter le dossier..."
// gallery button and the automatic periodic backup — kept in one place so
// the copy logic (and its Corbeille-exclusion rule) can't drift apart.
namespace FileUtils {

bool copyFolderRecursively(const QString &sourceDir, const QString &destDir, const QString &excludeSubfolderName);

} // namespace FileUtils
