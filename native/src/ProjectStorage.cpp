
#include <QJsonDocument>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QScopeGuard>
#include <QTemporaryFile>
#include <QFileInfo>
#include <QDir>
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

#ifdef Q_OS_WIN
#include <windows.h>
#endif


#include "ProjectStorage.h"

namespace MarchCraft::ProjectStorage {
namespace {
bool writeDatabase(const QString &path, const QJsonObject &document, QString *error)
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

bool replaceAtomically(const QString &stagedPath, const QString &destination, QString *error)
{
#ifdef Q_OS_WIN
    const std::wstring staged = stagedPath.toStdWString();
    const std::wstring target = destination.toStdWString();
    const bool exists = QFileInfo::exists(destination);
    const BOOL replaced = exists
        ? ReplaceFileW(target.c_str(), staged.c_str(), nullptr, REPLACEFILE_IGNORE_MERGE_ERRORS, nullptr, nullptr)
        : MoveFileExW(staged.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
    if (replaced) return true;
    if (error) *error = QStringLiteral("Could not atomically replace project (Windows error %1)").arg(GetLastError());
    return false;
#else
    if (QFileInfo::exists(destination) && !QFile::remove(destination)) {
        if (error) *error = QStringLiteral("Could not replace existing project");
        return false;
    }
    if (QFile::rename(stagedPath, destination)) return true;
    if (error) *error = QStringLiteral("Could not move staged project into place");
    return false;
#endif
}
}

bool writeSqliteProject(const QString &path, const QJsonObject &document, QString *error)
{
    const QFileInfo target(path);
    QDir directory(target.absolutePath());
    if (!directory.exists() && !directory.mkpath(QStringLiteral("."))) {
        if (error) *error = QStringLiteral("Could not create project directory");
        return false;
    }
    QString stagedPath;
    {
        QTemporaryFile staged(directory.filePath(target.fileName() + QStringLiteral(".staged-XXXXXX")));
        staged.setAutoRemove(false);
        if (!staged.open()) {
            if (error) *error = QStringLiteral("Could not create staged project file");
            return false;
        }
        stagedPath = staged.fileName();
        staged.close();
    }
    auto cleanup = qScopeGuard([&] { QFile::remove(stagedPath); });
    if (!writeDatabase(stagedPath, document, error)) return false;
    QJsonObject verified;
    QString verifyError;
    if (!readSqliteProject(stagedPath, &verified, &verifyError) || verified != document) {
        if (error) *error = QStringLiteral("Staged project validation failed: %1").arg(verifyError);
        return false;
    }
    if (!replaceAtomically(stagedPath, path, error)) return false;
    cleanup.dismiss();
    return true;
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
