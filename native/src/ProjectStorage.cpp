
#include <QJsonDocument>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QSizeF>
#include <QSet>
#include <QUuid>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <functional>
#include <numbers>
#include <numeric>
#include <utility>


#include "ProjectStorage.h"

namespace MarchCraft::ProjectStorage {
bool writeSqliteProject(const QString &path, const QJsonObject &document, QString *error)
{
    const QString connection = QStringLiteral("marchcraft-write-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    bool ok = false;
    {
        auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
        db.setDatabaseName(path);
        if (!db.open()) { if (error) *error = db.lastError().text(); }
        else if (!db.transaction()) { if (error) *error = db.lastError().text(); }
        else {
            QSqlQuery query(db);
            ok = query.exec(QStringLiteral("CREATE TABLE IF NOT EXISTS project_document (id INTEGER PRIMARY KEY CHECK(id=1), schema_version INTEGER NOT NULL, json BLOB NOT NULL, updated_utc TEXT NOT NULL)"));
            if (ok) {
                query.prepare(QStringLiteral("INSERT OR REPLACE INTO project_document(id,schema_version,json,updated_utc) VALUES(1,?,?,datetime('now'))"));
                query.addBindValue(document.value(QStringLiteral("version")).toInt());
                query.addBindValue(QJsonDocument(document).toJson(QJsonDocument::Compact));
                ok = query.exec();
            }
            if (ok) ok = db.commit(); else db.rollback();
            if (!ok && error) *error = query.lastError().text();
        }
        db.close();
    }
    QSqlDatabase::removeDatabase(connection);
    return ok;
}

bool readSqliteProject(const QString &path, QJsonObject *document, QString *error)
{
    const QString connection = QStringLiteral("marchcraft-read-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    bool ok = false;
    {
        auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
        db.setConnectOptions(QStringLiteral("QSQLITE_OPEN_READONLY")); db.setDatabaseName(path);
        if (!db.open()) { if (error) *error = db.lastError().text(); }
        else {
            QSqlQuery query(QStringLiteral("SELECT json FROM project_document WHERE id=1"), db);
            if (query.next()) {
                const auto parsed = QJsonDocument::fromJson(query.value(0).toByteArray());
                if (parsed.isObject()) { *document = parsed.object(); ok = true; }
            }
            if (!ok && error) *error = query.lastError().text().isEmpty()
                ? QStringLiteral("Project database has no readable document") : query.lastError().text();
        }
        db.close();
    }
    QSqlDatabase::removeDatabase(connection);
    return ok;
}

}
