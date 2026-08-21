#include "WorkspaceController.h"

#include <QBuffer>
#include <QDateTime>
#include <QDir>
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
constexpr int MaximumRecentProjects = 8;

QByteArray startupWav()
{
    constexpr int sampleRate = 44100;
    constexpr double durationSeconds = 0.62;
    const int sampleCount = static_cast<int>(sampleRate * durationSeconds);
    QByteArray pcm(sampleCount * 2, Qt::Uninitialized);
    auto *samples = reinterpret_cast<qint16 *>(pcm.data());
    for (int i = 0; i < sampleCount; ++i) {
        const double t = static_cast<double>(i) / sampleRate;
        const double attack = qMin(1.0, t / 0.035);
        const double release = qBound(0.0, (durationSeconds - t) / 0.24, 1.0);
        const double envelope = attack * release * release;
        const double first = std::sin(2.0 * std::numbers::pi * 523.25 * t);
        const double second = std::sin(2.0 * std::numbers::pi * 659.25 * t) * qBound(0.0, (t - 0.10) / 0.10, 1.0);
        const double shimmer = std::sin(2.0 * std::numbers::pi * 1046.5 * t) * 0.12;
        samples[i] = qToLittleEndian<qint16>(static_cast<qint16>(
            qBound(-1.0, (first * 0.48 + second * 0.34 + shimmer) * envelope, 1.0) * 32767.0));
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
}

WorkspaceController::WorkspaceController(QObject *parent)
    : QObject(parent)
{
    QSettings settings(QSettings::NativeFormat, QSettings::UserScope,
                       QStringLiteral("MarchCraft"), QStringLiteral("MarchCraft"));
    m_startupSoundEnabled = settings.value(QString::fromLatin1(StartupSoundKey), true).toBool();
    m_startupSound.setVolume(0.22);
    refreshRecentProjects();
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
    const QString path = root + QStringLiteral("/marchcraft-startup.wav");
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
