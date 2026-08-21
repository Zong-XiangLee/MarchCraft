#pragma once

#include <QObject>
#include <QSoundEffect>
#include <QVariantList>

class WorkspaceController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList recentProjects READ recentProjects NOTIFY recentProjectsChanged)
    Q_PROPERTY(bool startupSoundEnabled READ startupSoundEnabled WRITE setStartupSoundEnabled
               NOTIFY startupSoundEnabledChanged)

public:
    explicit WorkspaceController(QObject *parent = nullptr);

    QVariantList recentProjects() const { return m_recentProjects; }
    bool startupSoundEnabled() const { return m_startupSoundEnabled; }
    void setStartupSoundEnabled(bool enabled);

    Q_INVOKABLE void recordRecentProject(const QString &urlOrPath, const QString &displayName = {});
    Q_INVOKABLE void removeRecentProject(const QString &urlOrPath);
    Q_INVOKABLE void refreshRecentProjects();
    Q_INVOKABLE void playStartupSound();

signals:
    void recentProjectsChanged();
    void startupSoundEnabledChanged();

private:
    static QString localPath(const QString &urlOrPath);
    static QString normalizedPath(const QString &urlOrPath);
    void persistRecentProjects() const;
    QString ensureStartupSound();

    QVariantList m_recentProjects;
    bool m_startupSoundEnabled = true;
    QSoundEffect m_startupSound;
};
