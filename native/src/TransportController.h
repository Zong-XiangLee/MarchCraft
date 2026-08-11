#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>

class DrillProject;
class MidiSynthEngine;
class QAudioOutput;
class QMediaPlayer;

class TransportController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY stateChanged)
    Q_PROPERTY(qint64 currentTick READ currentTick NOTIFY positionChanged)
    Q_PROPERTY(qint64 durationTick READ durationTick NOTIFY positionChanged)
    Q_PROPERTY(double currentMs READ currentMs NOTIFY positionChanged)
    Q_PROPERTY(double durationMs READ durationMs NOTIFY positionChanged)
    Q_PROPERTY(double normalizedPosition READ normalizedPosition NOTIFY positionChanged)
    Q_PROPERTY(qint64 loopStartTick READ loopStartTick NOTIFY loopChanged)
    Q_PROPERTY(qint64 loopEndTick READ loopEndTick NOTIFY loopChanged)
    Q_PROPERTY(QString audioStatus READ audioStatus NOTIFY audioStatusChanged)
    Q_PROPERTY(bool synthAvailable READ synthAvailable NOTIFY audioStatusChanged)
    Q_PROPERTY(int underrunCount READ underrunCount NOTIFY audioStatusChanged)
public:
    explicit TransportController(DrillProject *project, QObject *parent = nullptr);
    QString state() const { return m_state; }
    bool playing() const { return m_state == QStringLiteral("playing"); }
    qint64 currentTick() const { return m_tick; }
    qint64 durationTick() const;
    double currentMs() const;
    double durationMs() const;
    double normalizedPosition() const;
    qint64 loopStartTick() const;
    qint64 loopEndTick() const;
    QString audioStatus() const;
    bool synthAvailable() const;
    int underrunCount() const;

    Q_INVOKABLE void playPause();
    Q_INVOKABLE void playFromSelection();
    Q_INVOKABLE void playCurrentTransition();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void seekTick(qint64 tick);
    Q_INVOKABLE void seekNormalized(double position);
    Q_INVOKABLE void firstSet();
    Q_INVOKABLE void previousSet();
    Q_INVOKABLE void nextSet();
    Q_INVOKABLE void toggleLoop();
    Q_INVOKABLE void refreshMusic();

signals:
    void stateChanged();
    void positionChanged();
    void loopChanged();
    void audioStatusChanged();

private:
    void startAt(qint64 tick);
    void startOpeningHold(bool reset = true);
    void updatePosition();
    void applyTick(qint64 tick);
    qint64 setTick(int index) const;
    double musicMsAt(qint64 tick) const;
    qint64 tickAtMusicMs(double ms) const;
    void syncSource();

    DrillProject *m_project;
    MidiSynthEngine *m_synth;
    QMediaPlayer *m_player;
    QAudioOutput *m_audioOutput;
    QTimer m_timer;
    QElapsedTimer m_clock;
    QString m_state{QStringLiteral("stopped")};
    qint64 m_tick = 0;
    qint64 m_playEndTick = 0;
    double m_baseMusicMs = 0.0;
    QString m_loadedMusicHash;
    bool m_openingHoldActive = false;
    double m_openingHoldElapsedMs = 0.0;
};
