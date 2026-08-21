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
    Q_PROPERTY(bool systemAnimationsEnabled READ systemAnimationsEnabled
               NOTIFY systemAnimationsEnabledChanged)

public:
    explicit WorkspaceController(QObject *parent = nullptr);

    QVariantList recentProjects() const { return m_recentProjects; }
    bool startupSoundEnabled() const { return m_startupSoundEnabled; }
    bool systemAnimationsEnabled() const { return m_systemAnimationsEnabled; }
    void setStartupSoundEnabled(bool enabled);

    Q_INVOKABLE void recordRecentProject(const QString &urlOrPath, const QString &displayName = {});
    Q_INVOKABLE void removeRecentProject(const QString &urlOrPath);
    Q_INVOKABLE void refreshRecentProjects();
    Q_INVOKABLE void playStartupSound();
    Q_INVOKABLE void refreshSystemPreferences();

signals:
    void recentProjectsChanged();
    void startupSoundEnabledChanged();
    void systemAnimationsEnabledChanged();

private:
    static QString localPath(const QString &urlOrPath);
    static QString normalizedPath(const QString &urlOrPath);
    void persistRecentProjects() const;
    QString ensureStartupSound();

    QVariantList m_recentProjects;
    bool m_startupSoundEnabled = true;
    bool m_systemAnimationsEnabled = true;
    QSoundEffect m_startupSound;
};
