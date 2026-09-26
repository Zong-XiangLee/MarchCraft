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
    Q_PROPERTY(bool recoveryAvailable READ recoveryAvailable NOTIFY recoveryChanged)
    Q_PROPERTY(QString recoveryPath READ recoveryPath NOTIFY recoveryChanged)
    Q_PROPERTY(QString recoveryDescription READ recoveryDescription NOTIFY recoveryChanged)

public:
    explicit WorkspaceController(QObject *parent = nullptr);

    QVariantList recentProjects() const { return m_recentProjects; }
    bool startupSoundEnabled() const { return m_startupSoundEnabled; }
    bool systemAnimationsEnabled() const { return m_systemAnimationsEnabled; }
    bool recoveryAvailable() const { return m_recoveryAvailable; }
    QString recoveryPath() const { return m_recoveryPath; }
    QString recoveryDescription() const { return m_recoveryDescription; }
    void setStartupSoundEnabled(bool enabled);

    Q_INVOKABLE void recordRecentProject(const QString &urlOrPath, const QString &displayName = {});
    Q_INVOKABLE void removeRecentProject(const QString &urlOrPath);
    Q_INVOKABLE void refreshRecentProjects();
    Q_INVOKABLE void playStartupSound();
    Q_INVOKABLE void refreshSystemPreferences();
    Q_INVOKABLE void refreshRecovery();
    Q_INVOKABLE bool discardRecovery();
    Q_INVOKABLE QString workspaceForProject(const QString &urlOrPath) const;
    Q_INVOKABLE void rememberWorkspace(const QString &urlOrPath, const QString &workspace);

signals:
    void recentProjectsChanged();
    void startupSoundEnabledChanged();
    void systemAnimationsEnabledChanged();
    void recoveryChanged();

private:
    static QString localPath(const QString &urlOrPath);
    static QString normalizedPath(const QString &urlOrPath);
    void persistRecentProjects() const;
    QString ensureStartupSound();

    QVariantList m_recentProjects;
    bool m_startupSoundEnabled = true;
    bool m_systemAnimationsEnabled = true;
    bool m_recoveryAvailable = false;
    QString m_recoveryPath;
    QString m_recoveryDescription;
    QSoundEffect m_startupSound;
};
