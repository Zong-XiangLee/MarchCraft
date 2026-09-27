#include "WorkspaceController.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>
#include <QtEndian>
#include <cmath>
#include <numbers>
#include <utility>

namespace {
constexpr auto RecentProjectsKey = "workspace/recentProjects";
constexpr auto StartupSoundKey = "workspace/startupSoundEnabled";
constexpr auto ProjectWorkspacePrefix = "workspace/project/";
constexpr int MaximumRecentProjects = 8;

QByteArray startupWav()
{
    constexpr int sampleRate = 44100;
    constexpr double durationSeconds = 0.92;
    const int sampleCount = static_cast<int>(sampleRate * durationSeconds);
    QByteArray pcm(sampleCount * 2, Qt::Uninitialized);
    auto *samples = reinterpret_cast<qint16 *>(pcm.data());
    for (int i = 0; i < sampleCount; ++i) {
        const double t = static_cast<double>(i) / sampleRate;
        auto bell = [t](double start, double frequency, double gain) {
            const double local = t - start;
            if (local < 0.0) return 0.0;
            const double attack = qMin(1.0, local / 0.018);
            const double decay = std::exp(-local * 5.1);
            const double fundamental = std::sin(2.0 * std::numbers::pi * frequency * local);
            const double warmth = 0.24 * std::sin(2.0 * std::numbers::pi * frequency * 2.0 * local);
            const double air = 0.07 * std::sin(2.0 * std::numbers::pi * frequency * 3.0 * local);
            return gain * attack * decay * (fundamental + warmth + air);
        };
        const double chord = bell(0.00, 392.00, 0.34)
                           + bell(0.15, 493.88, 0.30)
                           + bell(0.32, 587.33, 0.28);
        const double finish = qBound(0.0, (durationSeconds - t) / 0.10, 1.0);
        samples[i] = qToLittleEndian<qint16>(static_cast<qint16>(
            qBound(-0.92, chord * finish, 0.92) * 32767.0));
    }

    QByteArray wav;
    QBuffer buffer(&wav);
    buffer.open(QIODevice::WriteOnly);
    auto write16 = [&buffer](quint16 value) {
        value = qToLittleEndian(value); buffer.write(reinterpret_cast<const char *>(&value), 2);
    };
    auto write32 = [&buffer](quint32 value) {
        value = qToLittleEndian(value); buffer.write(reinterpret_cast<const char *>(&value), 4);
    };
    buffer.write("RIFF", 4); write32(36 + pcm.size()); buffer.write("WAVE", 4);
    buffer.write("fmt ", 4); write32(16); write16(1); write16(1); write32(sampleRate);
    write32(sampleRate * 2); write16(2); write16(16);
    buffer.write("data", 4); write32(pcm.size()); buffer.write(pcm);
    return wav;
}

QString projectWorkspaceKey(const QString &path)
{
    const QByteArray digest = QCryptographicHash::hash(path.toUtf8(), QCryptographicHash::Sha256).toHex();
    return QString::fromLatin1(ProjectWorkspacePrefix) + QString::fromLatin1(digest);
}
}

WorkspaceController::WorkspaceController(QObject *parent)
    : QObject(parent)
{
    QSettings settings(QSettings::NativeFormat, QSettings::UserScope,
                       QStringLiteral("MarchCraft"), QStringLiteral("MarchCraft"));
    m_startupSoundEnabled = settings.value(QString::fromLatin1(StartupSoundKey), true).toBool();
    m_startupSound.setVolume(0.34);
    refreshSystemPreferences();
    refreshRecentProjects();
    refreshRecovery();
}

void WorkspaceController::refreshSystemPreferences()
{
    bool enabled = true;
#ifdef Q_OS_WIN
    QSettings animationSettings(QStringLiteral("HKEY_CURRENT_USER\\Control Panel\\Desktop\\WindowMetrics"),
                                QSettings::NativeFormat);
    enabled = animationSettings.value(QStringLiteral("MinAnimate"), QStringLiteral("1")).toString() != QStringLiteral("0");
#endif
    if (enabled == m_systemAnimationsEnabled) return;
    m_systemAnimationsEnabled = enabled;
    emit systemAnimationsEnabledChanged();
}

void WorkspaceController::setStartupSoundEnabled(bool enabled)
{
    if (enabled == m_startupSoundEnabled) return;
    m_startupSoundEnabled = enabled;
    QSettings settings(QSettings::NativeFormat, QSettings::UserScope,
                       QStringLiteral("MarchCraft"), QStringLiteral("MarchCraft"));
    settings.setValue(QString::fromLatin1(StartupSoundKey), enabled);
    settings.sync();
    emit startupSoundEnabledChanged();
}

QString WorkspaceController::localPath(const QString &urlOrPath)
{
    const QUrl url(urlOrPath);
    return url.isLocalFile() ? url.toLocalFile() : urlOrPath;
}

QString WorkspaceController::normalizedPath(const QString &urlOrPath)
{
    if (urlOrPath.trimmed().isEmpty())
        return {};
    QFileInfo info(localPath(urlOrPath));
    const QString canonical = info.canonicalFilePath();
    return QDir::cleanPath(canonical.isEmpty() ? info.absoluteFilePath() : canonical);
}

void WorkspaceController::recordRecentProject(const QString &urlOrPath, const QString &displayName)
{
    const QString path = normalizedPath(urlOrPath);
    const QFileInfo info(path);
    if (!info.exists() || info.suffix().compare(QStringLiteral("marchcraft"), Qt::CaseInsensitive) != 0)
        return;

    QVariantList next;
    QVariantMap entry;
    entry.insert(QStringLiteral("path"), path);
    entry.insert(QStringLiteral("name"), displayName.trimmed().isEmpty() ? info.completeBaseName()
                                                                         : displayName.trimmed());
    entry.insert(QStringLiteral("folder"), QDir::toNativeSeparators(info.absolutePath()));
    entry.insert(QStringLiteral("lastOpened"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    next.push_back(entry);
    for (const QVariant &value : std::as_const(m_recentProjects)) {
        const QVariantMap recent = value.toMap();
        if (normalizedPath(recent.value(QStringLiteral("path")).toString()) == path) continue;
        if (next.size() >= MaximumRecentProjects) break;
        next.push_back(recent);
    }
    m_recentProjects = next;
    persistRecentProjects();
    emit recentProjectsChanged();
}

void WorkspaceController::removeRecentProject(const QString &urlOrPath)
{
    const QString path = normalizedPath(urlOrPath);
    QVariantList next;
    for (const QVariant &value : std::as_const(m_recentProjects)) {
        const QVariantMap recent = value.toMap();
        if (normalizedPath(recent.value(QStringLiteral("path")).toString()) != path)
            next.push_back(recent);
    }
    if (next == m_recentProjects) return;
    m_recentProjects = next;
    persistRecentProjects();
    emit recentProjectsChanged();
}

void WorkspaceController::refreshRecentProjects()
{
    QSettings settings(QSettings::NativeFormat, QSettings::UserScope,
                       QStringLiteral("MarchCraft"), QStringLiteral("MarchCraft"));
    const QByteArray stored = settings.value(QString::fromLatin1(RecentProjectsKey)).toByteArray();
    const QJsonArray array = QJsonDocument::fromJson(stored).array();
    QVariantList next;
    for (const QJsonValue &value : array) {
        const QVariantMap recent = value.toObject().toVariantMap();
        const QString path = normalizedPath(recent.value(QStringLiteral("path")).toString());
        const QFileInfo info(path);
        if (!info.exists() || info.suffix().compare(QStringLiteral("marchcraft"), Qt::CaseInsensitive) != 0)
            continue;
        QVariantMap refreshed = recent;
        refreshed.insert(QStringLiteral("path"), path);
        refreshed.insert(QStringLiteral("folder"), QDir::toNativeSeparators(info.absolutePath()));
        if (refreshed.value(QStringLiteral("name")).toString().trimmed().isEmpty())
            refreshed.insert(QStringLiteral("name"), info.completeBaseName());
        next.push_back(refreshed);
        if (next.size() >= MaximumRecentProjects) break;
    }
    const bool changed = next != m_recentProjects;
    m_recentProjects = next;
    persistRecentProjects();
    if (changed) emit recentProjectsChanged();
}

void WorkspaceController::refreshRecovery()
{
    const QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + QStringLiteral("/recovery.marchcraft");
    const QFileInfo info(path);
    const bool available = info.exists() && info.isFile() && info.size() > 0;
    const QString description = available
        ? QStringLiteral("Autosaved %1").arg(info.lastModified().toLocalTime().toString(QStringLiteral("MMM d, h:mm AP")))
        : QString{};
    if (available == m_recoveryAvailable && path == m_recoveryPath
        && description == m_recoveryDescription)
        return;
    m_recoveryAvailable = available;
    m_recoveryPath = path;
    m_recoveryDescription = description;
    emit recoveryChanged();
}

bool WorkspaceController::discardRecovery()
{
    const QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
        + QStringLiteral("/recovery.marchcraft");
    if (QFileInfo::exists(path) && !QFile::remove(path))
        return false;
    refreshRecovery();
    return true;
}

QString WorkspaceController::workspaceForProject(const QString &urlOrPath) const
{
    const QString path = normalizedPath(urlOrPath);
    if (path.isEmpty()) return QStringLiteral("editor");
    QSettings settings(QSettings::NativeFormat, QSettings::UserScope,
                       QStringLiteral("MarchCraft"), QStringLiteral("MarchCraft"));
    const QString workspace = settings.value(projectWorkspaceKey(path), QStringLiteral("editor")).toString();
    static const QStringList valid{QStringLiteral("roster"), QStringLiteral("music"),
                                   QStringLiteral("editor"), QStringLiteral("review")};
    return valid.contains(workspace) ? workspace : QStringLiteral("editor");
}

void WorkspaceController::rememberWorkspace(const QString &urlOrPath, const QString &workspace)
{
    const QString path = normalizedPath(urlOrPath);
    static const QStringList valid{QStringLiteral("roster"), QStringLiteral("music"),
                                   QStringLiteral("editor"), QStringLiteral("review")};
    if (path.isEmpty() || !valid.contains(workspace)) return;
    QSettings settings(QSettings::NativeFormat, QSettings::UserScope,
                       QStringLiteral("MarchCraft"), QStringLiteral("MarchCraft"));
    settings.setValue(projectWorkspaceKey(path), workspace);
    settings.sync();
}

void WorkspaceController::persistRecentProjects() const
{
    QJsonArray array;
    for (const QVariant &value : m_recentProjects)
        array.push_back(QJsonObject::fromVariantMap(value.toMap()));
    QSettings settings(QSettings::NativeFormat, QSettings::UserScope,
                       QStringLiteral("MarchCraft"), QStringLiteral("MarchCraft"));
    settings.setValue(QString::fromLatin1(RecentProjectsKey), QJsonDocument(array).toJson(QJsonDocument::Compact));
    settings.sync();
}

QString WorkspaceController::ensureStartupSound()
{
    const QString root = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QDir().mkpath(root);
    const QString path = root + QStringLiteral("/marchcraft-startup-v2.wav");
    QFileInfo info(path);
    if (info.exists() && info.size() > 44) return path;
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return {};
    file.write(startupWav());
    return file.commit() ? path : QString{};
}

void WorkspaceController::playStartupSound()
{
    if (!m_startupSoundEnabled) return;
    const QString path = ensureStartupSound();
    if (path.isEmpty()) return;
    m_startupSound.setSource(QUrl::fromLocalFile(path));
    m_startupSound.play();
}
