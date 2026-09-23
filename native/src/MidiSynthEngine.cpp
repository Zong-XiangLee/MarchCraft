#include "MidiSynthEngine.h"

#include <QAudioDevice>
#include <QAudioSink>
#include <QCoreApplication>
#include <QFileInfo>
#include <QMediaDevices>
#include <QMutexLocker>

#include <algorithm>
#include <cstring>

MidiSynthWorker::MidiSynthWorker(QObject *parent, bool offline) : QObject(parent)
{
    m_format.setSampleRate(48000);
    m_format.setChannelCount(2);
    m_format.setSampleFormat(QAudioFormat::Float);
    if (!loadLibrary()) return;
    QAudioDevice device = QMediaDevices::defaultAudioOutput();
    if (!offline && device.isNull()) { setStatus(QStringLiteral("No audio output device is available")); return; }
    if (!offline && !device.isFormatSupported(m_format)) m_format = device.preferredFormat();
    if (m_format.sampleFormat() != QAudioFormat::Float || m_format.channelCount() != 2) {
        setStatus(QStringLiteral("Audio device does not support stereo floating-point playback")); return;
    }
    m_settings = p_newSettings();
    if (!m_settings) { setStatus(QStringLiteral("FluidSynth settings could not be created")); return; }
    p_settingsSetNum(m_settings, "synth.sample-rate", m_format.sampleRate());
    m_synth = p_newSynth(m_settings);
    if (!m_synth) { setStatus(QStringLiteral("FluidSynth could not be initialized")); return; }
    const QString font = QCoreApplication::applicationDirPath()
        + QStringLiteral("/soundfonts/GeneralUser-GS.sf2");
    if (!QFileInfo::exists(font) || p_sfLoad(m_synth, font.toUtf8().constData(), 1) < 0) {
        setStatus(QStringLiteral("Bundled GeneralUser GS SoundFont is missing")); return;
    }
    if (offline) { m_available = true; setStatus(QStringLiteral("Offline MIDI synth ready")); return; }
    m_sink = new QAudioSink(device, m_format, this);
    // About 100 ms of headroom, serviced independently of GUI/render stalls.
    m_sink->setBufferSize(m_format.bytesForFrames(m_format.sampleRate() / 10));
    m_positionTimer.setInterval(10);
    m_positionTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_positionTimer, &QTimer::timeout, this, [this] {
        if (m_sink->state() == QAudio::ActiveState || m_sink->state() == QAudio::IdleState)
            m_positionMs.store(m_startMs + m_sink->processedUSecs() / 1000.0);
    });
    m_positionTimer.start();
    connect(m_sink, &QAudioSink::stateChanged, this, [this](QAudio::State state) {
        if (state == QAudio::IdleState && m_sink->error() == QAudio::UnderrunError) {
            ++m_underruns; emit statusChanged();
        }
    });
    m_stream.open(QIODevice::ReadOnly);
    m_available = true;
    setStatus(QStringLiteral("MIDI Synth ready"));
}

MidiSynthWorker::~MidiSynthWorker()
{
    m_positionTimer.stop();
    if (m_sink) m_sink->stop();
    m_stream.close();
    if (m_synth && p_deleteSynth) p_deleteSynth(m_synth);
    if (m_settings && p_deleteSettings) p_deleteSettings(m_settings);
}

bool MidiSynthWorker::loadLibrary()
{
    const QString base = QCoreApplication::applicationDirPath();
#ifdef Q_OS_WIN
    const QStringList candidates{base + QStringLiteral("/libfluidsynth-3.dll"),
        base + QStringLiteral("/fluidsynth/libfluidsynth-3.dll"),
        base + QStringLiteral("/fluidsynth/libfluidsynth.dll"), QStringLiteral("libfluidsynth-3")};
#elif defined(Q_OS_MACOS)
    const QStringList candidates{base + QStringLiteral("/fluidsynth/libfluidsynth.3.dylib"), QStringLiteral("fluidsynth")};
#else
    const QStringList candidates{base + QStringLiteral("/fluidsynth/libfluidsynth.so.3"), QStringLiteral("fluidsynth")};
#endif
    for (const QString &candidate : candidates) { m_library.setFileName(candidate); if (m_library.load()) break; }
    if (!m_library.isLoaded()) { setStatus(QStringLiteral("Bundled FluidSynth runtime is missing")); return false; }
#define RESOLVE(name, member) member = reinterpret_cast<decltype(member)>(m_library.resolve(name)); if (!member) return false
    RESOLVE("new_fluid_settings", p_newSettings); RESOLVE("delete_fluid_settings", p_deleteSettings);
    RESOLVE("fluid_settings_setnum", p_settingsSetNum); RESOLVE("new_fluid_synth", p_newSynth);
    RESOLVE("delete_fluid_synth", p_deleteSynth); RESOLVE("fluid_synth_sfload", p_sfLoad);
    RESOLVE("fluid_synth_write_float", p_writeFloat); RESOLVE("fluid_synth_noteon", p_noteOn);
    RESOLVE("fluid_synth_noteoff", p_noteOff); RESOLVE("fluid_synth_cc", p_cc);
    RESOLVE("fluid_synth_program_change", p_programChange); RESOLVE("fluid_synth_pitch_bend", p_pitchBend);
    RESOLVE("fluid_synth_channel_pressure", p_channelPressure); RESOLVE("fluid_synth_system_reset", p_systemReset);
    RESOLVE("fluid_synth_set_gain", p_setGain);
#undef RESOLVE
    return true;
}

bool MidiSynthWorker::load(const MarchCraft::MusicDocument &document)
{
    if (m_sink) m_sink->reset();
    QMutexLocker lock(&m_mutex);
    m_document = document; m_eventIndex = 0; m_startTick = 0; m_startMs = 0.0;
    m_eventFrames.clear(); m_eventFrames.reserve(document.playbackEvents.size());
    for (const auto &event : document.playbackEvents)
        m_eventFrames.push_back(qRound64(document.millisecondsAt(event.tick) * m_format.sampleRate() / 1000.0));
    m_renderedFrames = 0; m_positionMs.store(0.0); if (m_synth) p_systemReset(m_synth);
    return m_available && !m_document.playbackEvents.isEmpty();
}

void MidiSynthWorker::updateTracks(const QVector<MarchCraft::MusicTrack> &tracks)
{
    QMutexLocker lock(&m_mutex); m_document.tracks = tracks;
}

void MidiSynthWorker::play()
{
    if (!m_available || !m_sink) return;
    if (m_sink->state() == QAudio::SuspendedState) m_sink->resume(); else m_sink->start(&m_stream);
}

void MidiSynthWorker::pause() { if (m_sink) m_sink->suspend(); }

void MidiSynthWorker::stop()
{
    if (m_sink) m_sink->stop();
    seekTick(m_startTick);
}

void MidiSynthWorker::seekTick(qint64 tick)
{
    // Discard queued audio from the previous position before rebuilding MIDI state.
    if (m_sink) m_sink->reset();
    QMutexLocker lock(&m_mutex);
    tick = qBound<qint64>(0, tick, m_document.durationTick); restoreState(tick);
    m_startTick = tick; m_startMs = m_document.millisecondsAt(tick);
    m_renderedFrames = 0; m_positionMs.store(m_startMs);
}

void MidiSynthWorker::setGain(double gain)
{
    QMutexLocker lock(&m_mutex); if (m_synth) p_setGain(m_synth, float(qBound(0.0, gain, 1.0)));
}

double MidiSynthWorker::positionMs() const { return m_positionMs.load(); }

bool MidiSynthWorker::trackAudible(int track) const
{
    if (track < 0 || track >= m_document.tracks.size()) return true;
    const bool anySolo = std::any_of(m_document.tracks.cbegin(), m_document.tracks.cend(),
        [](const auto &item) { return item.solo; });
    return !m_document.tracks[track].muted && (!anySolo || m_document.tracks[track].solo);
}

void MidiSynthWorker::dispatch(const MarchCraft::MidiPlaybackEvent &event, bool notes)
{
    if (!m_synth) return;
    const int kind = event.status & 0xf0, channel = event.status & 0x0f;
    if ((kind == 0x80 || kind == 0x90) && !notes) return;
    if (kind == 0x90 && event.data2 > 0 && !trackAudible(event.track)) return;
    if (kind == 0x80 || (kind == 0x90 && event.data2 == 0)) p_noteOff(m_synth, channel, event.data1);
    else if (kind == 0x90) {
        const double volume = event.track < m_document.tracks.size() ? m_document.tracks[event.track].volume : 1.0;
        p_noteOn(m_synth, channel, event.data1, qBound(1, qRound(event.data2 * volume), 127));
    } else if (kind == 0xb0) p_cc(m_synth, channel, event.data1, event.data2);
    else if (kind == 0xc0) p_programChange(m_synth, channel, event.data1);
    else if (kind == 0xd0) p_channelPressure(m_synth, channel, event.data1);
    else if (kind == 0xe0) p_pitchBend(m_synth, channel, event.data1 | (event.data2 << 7));
}

void MidiSynthWorker::restoreState(qint64 tick)
{
    if (!m_synth) return;
    p_systemReset(m_synth); m_eventIndex = 0;
    bool active[16][128]{}; int velocity[16][128]{}; int activeTrack[16][128]{};
    while (m_eventIndex < m_document.playbackEvents.size()
           && m_document.playbackEvents[m_eventIndex].tick < tick) {
        const auto &event = m_document.playbackEvents[m_eventIndex++]; const int kind=event.status&0xf0, ch=event.status&0x0f;
        if (kind == 0x90 && event.data2 > 0) { active[ch][event.data1]=true; velocity[ch][event.data1]=event.data2; activeTrack[ch][event.data1]=event.track; }
        else if (kind == 0x80 || (kind == 0x90 && event.data2 == 0)) active[ch][event.data1]=false;
        else dispatch(event, false);
    }
    for (int ch=0; ch<16; ++ch) for (int note=0; note<128; ++note) if (active[ch][note])
        dispatch({tick, activeTrack[ch][note], quint8(0x90|ch), quint8(note), quint8(velocity[ch][note])}, true);
}

qint64 MidiSynthWorker::render(char *data, qint64 maxSize)
{
    const int frameBytes = int(sizeof(float) * 2); const int requested = int(maxSize / frameBytes);
    if (requested <= 0) return 0; std::memset(data, 0, size_t(requested * frameBytes));
    QMutexLocker lock(&m_mutex); if (!m_synth || m_document.playbackEvents.isEmpty()) return requested * frameBytes;
    float *samples = reinterpret_cast<float *>(data); int completed = 0;
    while (completed < requested) {
        const qint64 frame = qRound64(m_startMs * m_format.sampleRate() / 1000.0) + m_renderedFrames + completed;
        while (m_eventIndex < m_document.playbackEvents.size() && m_eventFrames[m_eventIndex] <= frame)
            dispatch(m_document.playbackEvents[m_eventIndex++], true);
        int frames = requested - completed;
        if (m_eventIndex < m_document.playbackEvents.size())
            frames = int(qBound<qint64>(qint64(1), m_eventFrames[m_eventIndex] - frame, qint64(frames)));
        p_writeFloat(m_synth, frames, samples + completed * 2, 0, 2,
                     samples + completed * 2, 1, 2); completed += frames;
    }
    m_renderedFrames += completed;
    return completed * frameBytes;
}

qint64 MidiSynthWorker::Stream::readData(char *data, qint64 maxSize) { return m_engine->render(data, maxSize); }

void MidiSynthWorker::setStatus(QString status)
{
    if (m_status == status) return; m_status = std::move(status); emit statusChanged();
}

MidiSynthEngine::MidiSynthEngine(QObject *parent) : QObject(parent), m_context(new QObject)
{
    m_audioThread.setObjectName(QStringLiteral("MarchCraft MIDI audio"));
    m_context->moveToThread(&m_audioThread);
    connect(&m_audioThread, &QThread::finished, m_context, &QObject::deleteLater);
    m_audioThread.start(QThread::HighPriority);
    QMetaObject::invokeMethod(m_context, [this] {
        m_worker = new MidiSynthWorker(m_context);
        m_status = m_worker->status();
        connect(m_worker, &MidiSynthWorker::statusChanged, m_worker, [this] {
            const QString status = m_worker->status();
            QMetaObject::invokeMethod(this, [this, status] { m_status = status; emit statusChanged(); }, Qt::QueuedConnection);
        });
    }, Qt::BlockingQueuedConnection);
}

MidiSynthEngine::~MidiSynthEngine()
{
    QMetaObject::invokeMethod(m_context, [this] { delete m_worker; m_worker = nullptr; }, Qt::BlockingQueuedConnection);
    m_audioThread.quit(); m_audioThread.wait();
}

bool MidiSynthEngine::load(const MarchCraft::MusicDocument &document)
{
    bool loaded = false;
    QMetaObject::invokeMethod(m_context, [this, &document, &loaded] { loaded = m_worker->load(document); }, Qt::BlockingQueuedConnection);
    return loaded;
}
void MidiSynthEngine::updateTracks(const QVector<MarchCraft::MusicTrack> &tracks)
{
    QMetaObject::invokeMethod(m_context, [this, &tracks] { m_worker->updateTracks(tracks); }, Qt::BlockingQueuedConnection);
}
void MidiSynthEngine::play() { QMetaObject::invokeMethod(m_context, [this] { m_worker->play(); }, Qt::BlockingQueuedConnection); }
void MidiSynthEngine::pause() { QMetaObject::invokeMethod(m_context, [this] { m_worker->pause(); }, Qt::BlockingQueuedConnection); }
void MidiSynthEngine::stop() { QMetaObject::invokeMethod(m_context, [this] { m_worker->stop(); }, Qt::BlockingQueuedConnection); }
void MidiSynthEngine::seekTick(qint64 tick)
{
    QMetaObject::invokeMethod(m_context, [this, tick] { m_worker->seekTick(tick); }, Qt::BlockingQueuedConnection);
}
void MidiSynthEngine::setGain(double gain)
{
    QMetaObject::invokeMethod(m_context, [this, gain] { m_worker->setGain(gain); }, Qt::BlockingQueuedConnection);
}
double MidiSynthEngine::positionMs() const { return m_worker->positionMs(); }
bool MidiSynthEngine::available() const { return m_worker->available(); }
int MidiSynthEngine::underruns() const { return m_worker->underruns(); }
