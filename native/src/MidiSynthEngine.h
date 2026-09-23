#pragma once

#include "MusicDocument.h"

#include <QAudioFormat>
#include <QIODevice>
#include <QLibrary>
#include <QMutex>
#include <QObject>
#include <QThread>
#include <QTimer>
#include <atomic>

class QAudioSink;

class MidiSynthWorker final : public QObject
{
    Q_OBJECT
public:
    explicit MidiSynthWorker(QObject *parent = nullptr, bool offline = false);
    qint64 renderOffline(char *data, qint64 size) { return render(data, size); }
    ~MidiSynthWorker() override;

    bool load(const MarchCraft::MusicDocument &document);
    void updateTracks(const QVector<MarchCraft::MusicTrack> &tracks);
    void play();
    void pause();
    void stop();
    void seekTick(qint64 tick);
    void setGain(double gain);
    double positionMs() const;
    bool available() const { return m_available; }
    QString status() const { return m_status; }
    int underruns() const { return m_underruns.load(); }

signals:
    void statusChanged();

private:
    class Stream final : public QIODevice {
    public:
        explicit Stream(MidiSynthWorker *engine) : m_engine(engine) {}
        bool isSequential() const override { return true; }
        qint64 bytesAvailable() const override { return 32768 + QIODevice::bytesAvailable(); }
        qint64 readData(char *data, qint64 maxSize) override;
        qint64 writeData(const char *, qint64) override { return -1; }
    private:
        MidiSynthWorker *m_engine;
    };

    bool loadLibrary();
    qint64 render(char *data, qint64 maxSize);
    void dispatch(const MarchCraft::MidiPlaybackEvent &event, bool notes);
    void restoreState(qint64 tick);
    bool trackAudible(int track) const;
    void setStatus(QString status);

    using Settings = void;
    using Synth = void;
    using NewSettings = Settings *(*)();
    using DeleteSettings = void (*)(Settings *);
    using SettingsSetNum = int (*)(Settings *, const char *, double);
    using NewSynth = Synth *(*)(Settings *);
    using DeleteSynth = void (*)(Synth *);
    using SfLoad = int (*)(Synth *, const char *, int);
    using WriteFloat = int (*)(Synth *, int, void *, int, int, void *, int, int);
    using Midi2 = int (*)(Synth *, int, int);
    using Midi3 = int (*)(Synth *, int, int, int);
    using Reset = int (*)(Synth *);
    using SetGain = void (*)(Synth *, float);

    QLibrary m_library;
    Settings *m_settings = nullptr;
    Synth *m_synth = nullptr;
    NewSettings p_newSettings = nullptr;
    DeleteSettings p_deleteSettings = nullptr;
    SettingsSetNum p_settingsSetNum = nullptr;
    NewSynth p_newSynth = nullptr;
    DeleteSynth p_deleteSynth = nullptr;
    SfLoad p_sfLoad = nullptr;
    WriteFloat p_writeFloat = nullptr;
    Midi3 p_noteOn = nullptr;
    Midi2 p_noteOff = nullptr;
    Midi3 p_cc = nullptr;
    Midi2 p_programChange = nullptr;
    Midi2 p_pitchBend = nullptr;
    Midi2 p_channelPressure = nullptr;
    Reset p_systemReset = nullptr;
    SetGain p_setGain = nullptr;

    MarchCraft::MusicDocument m_document;
    QAudioFormat m_format;
    QAudioSink *m_sink = nullptr;
    Stream m_stream{this};
    mutable QMutex m_mutex;
    qsizetype m_eventIndex = 0;
    qint64 m_startTick = 0;
    qint64 m_renderedFrames = 0;
    std::atomic<double> m_positionMs{0.0};
    bool m_available = false;
    QString m_status{QStringLiteral("MIDI synth unavailable")};
    std::atomic<int> m_underruns{0};
    QVector<qint64> m_eventFrames;
    double m_startMs = 0.0;
    QTimer m_positionTimer;
};

// GUI-facing facade. All synthesis and QAudioSink I/O live on m_audioThread.
class MidiSynthEngine final : public QObject
{
    Q_OBJECT
public:
    explicit MidiSynthEngine(QObject *parent = nullptr);
    ~MidiSynthEngine() override;
    bool load(const MarchCraft::MusicDocument &document);
    void updateTracks(const QVector<MarchCraft::MusicTrack> &tracks);
    void play();
    void pause();
    void stop();
    void seekTick(qint64 tick);
    void setGain(double gain);
    double positionMs() const;
    bool available() const;
    QString status() const { return m_status; }
    int underruns() const;

signals:
    void statusChanged();

private:
    QThread m_audioThread;
    QObject *m_context = nullptr;
    MidiSynthWorker *m_worker = nullptr;
    QString m_status;
};
