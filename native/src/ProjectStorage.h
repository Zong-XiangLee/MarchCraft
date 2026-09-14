#pragma once

#include "DrillTypes.h"
#include <QSizeF>

namespace MarchCraft::ProjectStorage {
bool writeSqliteProject(const QString &path, const QJsonObject &document, QString *error);
bool readSqliteProject(const QString &path, QJsonObject *document, QString *error);
}
