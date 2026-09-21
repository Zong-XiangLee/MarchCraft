#include "TransportController.h"

#include "DrillProject.h"
#include "MidiSynthEngine.h"

#include <QAudioOutput>
#include <QMediaPlayer>
#include <QUrl>
#include <cmath>

TransportController::TransportController(DrillProject *project, QObject *parent)
    : QObject(parent), m_project(project), m_synth(new MidiSynthEngine(this)),
      m_player(new QMediaPlayer(this)), m_audioOutput(new QAudioOutput(this))
{
    m_player->setAudioOutput(m_audioOutput);
    m_timer.setInterval(16);
    connect(&m_timer, &QTimer::timeout, this, &TransportController::updatePosition);
    connect(m_synth, &MidiSynthEngine::statusChanged, this, &TransportController::audioStatusChanged);
    connect(project, &DrillProject::musicChanged, this, &TransportController::refreshMusic);
    connect(project, &DrillProject::waveformChanged, this, &TransportController::positionChanged);
    connect(project, &DrillProject::transportSettingsChanged, this, [this] {
        m_synth->setGain(m_project->m_midiMasterVolume);
        syncSource();
        emit audioStatusChanged();
        emit loopChanged();
    });
    connect(project, &DrillProject::setRangeChanged, this, [this] {
        if (!m_navigating) navigateToSet(m_project->m_selectedSetEnd, true);
        emit loopChanged();
    });
    connect(project, &DrillProject::timingChanged, this, [this] {
        m_playEndTick = 0;
        seekMs(m_showMs);
    });
    connect(project, &DrillProject::setsChanged, this, [this] {
        m_playEndTick = 0;
        if (!playing() && !m_navigating && !m_project->m_sets.isEmpty())
            editSet(m_project->m_currentSet);
        emit positionChanged();
    });
    connect(project, &QAbstractItemModel::modelAboutToBeReset, this, [this] {
        m_timer.stop(); m_synth->pause(); m_player->stop();
        m_state = QStringLiteral("stopped"); m_tick = 0; m_showMs = 0.0;
        m_playEndTick = 0; m_scrubbing = false; m_resumeAfterScrub = false;
        m_sourceRunning = false; m_clock.invalidate();
        emit stateChanged(); emit positionChanged();
    });
    refreshMusic();
}

qint64 TransportController::durationTick() const
{
    const qint64 drill = m_project->m_sets.isEmpty() ? 0 : m_project->m_sets.last().startTick;
    return qMax(drill, m_project->m_music.durationTick);
}

double TransportController::durationMs() const
{
    return qMax(showMsAtTick(durationTick()), m_project->m_audioSource.isEmpty() ? 0.0
        : showMsAtAudioMs(m_project->audioDurationMs()));
}

double TransportController::normalizedPosition() const
{
    return durationMs() > 0 ? qBound(0.0, m_showMs / durationMs(), 1.0) : 0.0;
}

qint64 TransportController::setTick(int index) const
{
    return index >= 0 && index < m_project->m_sets.size() ? m_project->m_sets[index].startTick : 0;
}

double TransportController::setPositionMs(int index) const
{
    return index == 0 ? 0.0 : showMsAtTick(setTick(index));
}

qint64 TransportController::loopStartTick() const { return setTick(qMin(m_project->m_selectedSetStart, m_project->m_selectedSetEnd)); }
qint64 TransportController::loopEndTick() const { return setTick(qMax(m_project->m_selectedSetStart, m_project->m_selectedSetEnd)); }
QString TransportController::audioStatus() const
{
    return m_project->m_playbackSource == QStringLiteral("midi") ? m_synth->status()
        : m_project->m_playbackSource == QStringLiteral("rehearsal") ? QStringLiteral("Rehearsal audio") : QStringLiteral("Muted");
}
bool TransportController::synthAvailable() const { return m_synth->available(); }
int TransportController::underrunCount() const { return m_synth->underruns(); }

double TransportController::musicMsAt(qint64 tick) const
{
    tick = qMax<qint64>(0, tick);
    const auto &music = m_project->m_music;
    if (!music.loaded()) return m_project->millisecondsBetween(0, tick);
    const double tailBpm = music.tempos.isEmpty() ? 120.0 : music.tempos.last().bpm;
    return music.millisecondsAt(tick) + qMax<qint64>(0, tick - music.durationTick)
        * 60000.0 / (MarchCraft::TicksPerQuarter * tailBpm);
}

qint64 TransportController::tickAtMusicMs(double ms) const
{
    ms = qMax(0.0, ms);
    const auto &music = m_project->m_music;
    if (music.loaded()) {
        const double end = music.millisecondsAt(music.durationTick);
        if (ms <= end) return music.tickAtMilliseconds(ms);
        const double bpm = music.tempos.isEmpty() ? 120.0 : music.tempos.last().bpm;
        return music.durationTick + qRound64((ms - end) * MarchCraft::TicksPerQuarter * bpm / 60000.0);
    }
    qint64 low = 0, high = qMax<qint64>(1, durationTick());
    while (musicMsAt(high) < ms && high < (qint64(1) << 50)) high *= 2;
    while (low < high) {
        const qint64 mid = (low + high) / 2;
        if (musicMsAt(mid) < ms) low = mid + 1; else high = mid;
    }
    return low;
}

double TransportController::showMsAtTick(qint64 tick) const { return m_project->openingDurationMs() + musicMsAt(tick); }
qint64 TransportController::tickAtShowMs(double ms) const { return tickAtMusicMs(ms - m_project->openingDurationMs()); }

double TransportController::audioMsAtShowMs(double ms) const
{
    const double musicMs = ms - m_project->openingDurationMs();
    const auto &anchors = m_project->m_music.audioAnchors;
    if (anchors.isEmpty()) return musicMs + m_project->m_audioOffsetMs;
    if (anchors.size() == 1) return musicMs + anchors.first().audioMs
        - musicMsAt(anchors.first().musicTick) + m_project->m_audioOffsetMs;
    const qint64 tick = tickAtMusicMs(musicMs);
    int right = 1;
    while (right + 1 < anchors.size() && anchors[right].musicTick < tick) ++right;
    const auto &a = anchors[right - 1]; const auto &b = anchors[right];
    const double ratio = b.musicTick == a.musicTick ? 0.0 : double(tick - a.musicTick) / (b.musicTick - a.musicTick);
    return a.audioMs + ratio * (b.audioMs - a.audioMs) + m_project->m_audioOffsetMs;
}

double TransportController::showMsAtAudioMs(double ms) const
{
    ms -= m_project->m_audioOffsetMs;
    const auto &anchors = m_project->m_music.audioAnchors;
    if (anchors.isEmpty()) return m_project->openingDurationMs() + ms;
    if (anchors.size() == 1) return m_project->openingDurationMs() + ms - anchors.first().audioMs
        + musicMsAt(anchors.first().musicTick);
    int right = 1;
    while (right + 1 < anchors.size() && anchors[right].audioMs < ms) ++right;
    const auto &a = anchors[right - 1]; const auto &b = anchors[right];
    const double ratio = b.audioMs == a.audioMs ? 0.0 : (ms - a.audioMs) / (b.audioMs - a.audioMs);
    return showMsAtTick(qRound64(a.musicTick + ratio * (b.musicTick - a.musicTick)));
}

void TransportController::refreshMusic()
{
    const bool changed = m_loadedMusicHash != m_project->m_music.sourceHash;
    if (changed) { m_synth->load(m_project->m_music); m_loadedMusicHash = m_project->m_music.sourceHash; }
    m_synth->updateTracks(m_project->m_music.tracks);
    m_synth->setGain(m_project->m_midiMasterVolume);
    const QUrl source = m_project->m_audioSource.isEmpty() ? QUrl() : QUrl::fromLocalFile(m_project->m_audioSource);
    const bool audioChanged = source != m_player->source();
    if (audioChanged) m_player->setSource(source);
    QString mapping = QString::number(m_project->m_audioOffsetMs, 'g', 17);
    for (const auto &anchor : m_project->m_music.audioAnchors)
        mapping += QStringLiteral("/%1:%2").arg(anchor.musicTick).arg(anchor.audioMs, 0, 'g', 17);
    const bool mappingChanged = mapping != m_loadedAudioMapping;
    m_loadedAudioMapping = mapping;
    if (changed || audioChanged || mappingChanged) {
        m_showMs = qBound(0.0, m_showMs, durationMs());
        m_baseShowMs = m_showMs; m_clock.restart();
        syncSource();
    }
    emit positionChanged(); emit audioStatusChanged();
}

void TransportController::syncSource()
{
    m_synth->pause(); m_player->pause(); m_sourceRunning = false;
    if (!playing() || m_showMs < m_project->openingDurationMs()) return;
    if (m_project->m_playbackSource == QStringLiteral("midi") && m_synth->available()
        && !m_project->m_music.playbackEvents.isEmpty() && m_tick < m_project->m_music.durationTick) {
        m_synth->seekTick(m_tick); m_synth->play(); m_sourceRunning = true;
    } else if (m_project->m_playbackSource == QStringLiteral("rehearsal") && !m_project->m_audioSource.isEmpty()) {
        const double audioMs = audioMsAtShowMs(m_showMs);
        const double end = m_player->duration() > 0 ? m_player->duration() : m_project->audioDurationMs();
        if (audioMs >= 0 && (end <= 0 || audioMs < end)) {
            m_player->setPosition(qRound64(audioMs)); m_player->play(); m_sourceRunning = true;
        }
    }
}

void TransportController::resume()
{
    m_state = QStringLiteral("playing");
    m_baseShowMs = m_showMs; m_clock.restart();
    applyShowMs(m_showMs); syncSource(); m_timer.start(); emit stateChanged();
}

void TransportController::startAt(qint64 tick)
{
    applyShowMs(showMsAtTick(tick)); resume();
}

void TransportController::playPause()
{
    if (playing()) { pause(); return; }
    if (m_scrubbing) return;
    if (m_showMs >= durationMs()) { m_playEndTick = 0; applyShowMs(setPositionMs(m_project->m_selectedSetStart)); }
    resume();
}

void TransportController::playFromSelection()
{
    m_playEndTick = 0;
    applyShowMs(setPositionMs(qMin(m_project->m_selectedSetStart, m_project->m_selectedSetEnd)));
    resume();
}

void TransportController::playCurrentTransition()
{
    if (m_project->m_sets.size() < 2) return;
    const int destination = qBound(1, m_project->m_currentSet, m_project->m_sets.size() - 1);
    m_playEndTick = setTick(destination); startAt(setTick(destination - 1));
}

void TransportController::pause()
{
    if (!playing()) return;
    updatePosition(); m_timer.stop(); m_synth->pause(); m_player->pause();
    m_state = QStringLiteral("paused"); m_sourceRunning = false; emit stateChanged();
}

void TransportController::stop()
{
    pause(); m_timer.stop(); m_synth->pause(); m_player->pause();
    m_state = QStringLiteral("stopped"); m_playEndTick = 0;
    m_sourceRunning = false; m_resumeAfterScrub = false;
    emit stateChanged(); emit positionChanged();
}

void TransportController::checkLoopForSeek(double ms)
{
    if (m_project->m_loopEnabled && (ms < showMsAtTick(loopStartTick()) || ms >= showMsAtTick(loopEndTick()))) {
        // A navigation gesture is not a project edit or an undo command.
        m_project->m_loopEnabled = false;
        emit m_project->transportSettingsChanged(); emit loopChanged();
    }
}

void TransportController::seekMs(double milliseconds)
{
    if (!std::isfinite(milliseconds)) return;
    m_playEndTick = 0;
    const double target = qBound(0.0, milliseconds, durationMs());
    checkLoopForSeek(target);
    m_synth->pause(); m_player->pause();
    applyShowMs(target); m_baseShowMs = m_showMs; m_clock.restart(); syncSource();
}

void TransportController::seekTick(qint64 tick) { seekMs(showMsAtTick(qBound<qint64>(0, tick, durationTick()))); }
void TransportController::seekNormalized(double position) { seekMs(qBound(0.0, position, 1.0) * durationMs()); }

void TransportController::navigateToSet(int index, bool extend)
{
    if (m_project->m_sets.isEmpty()) return;
    index = qBound(0, index, m_project->m_sets.size() - 1);
    checkLoopForSeek(setPositionMs(index));
    m_navigating = true;
    if (m_project->m_loopEnabled && !extend) m_project->setCurrentSetIndex(index);
    else m_project->selectSetRange(index, extend);
    m_navigating = false;
    seekMs(setPositionMs(index));
    if (!playing()) { m_project->setPlaybackActive(false); m_project->setPlayhead(1.0); }
}

void TransportController::editSet(int index) { pause(); navigateToSet(index); }
void TransportController::beginScrub() { if (m_scrubbing) return; m_resumeAfterScrub = playing(); pause(); m_scrubbing = true; }
void TransportController::endScrub() { if (!m_scrubbing) return; m_scrubbing = false; if (m_resumeAfterScrub) resume(); m_resumeAfterScrub = false; }
void TransportController::firstSet() { navigateToSet(0); }
void TransportController::previousSet()
{
    int index = m_project->m_sets.size() - 1;
    while (index > 0 && setPositionMs(index) >= m_showMs - 0.5) --index;
    navigateToSet(index);
}
void TransportController::nextSet()
{
    int index = 0;
    while (index + 1 < m_project->m_sets.size() && setPositionMs(index) <= m_showMs + 0.5) ++index;
    navigateToSet(index);
}
void TransportController::toggleLoop() { m_project->setLoopEnabled(!m_project->m_loopEnabled); emit loopChanged(); }

void TransportController::applyTick(qint64 tick)
{
    m_tick = qMax<qint64>(0, tick);
    int destination = 0;
    while (destination + 1 < m_project->m_sets.size() && m_project->m_sets[destination + 1].startTick <= m_tick) ++destination;
    if (destination + 1 < m_project->m_sets.size()) {
        const qint64 a = setTick(destination), b = setTick(destination + 1);
        m_project->setPlaybackFrame(destination + 1, b > a ? double(m_tick - a) / (b - a) : 0.0);
    } else m_project->setPlaybackFrame(destination, 1.0);
}

void TransportController::applyShowMs(double ms)
{
    m_showMs = qBound(0.0, ms, durationMs());
    if (m_showMs < m_project->openingDurationMs()) { m_tick = 0; m_project->setPlaybackFrame(0, 1.0); }
    else applyTick(tickAtShowMs(m_showMs));
    emit positionChanged();
}

void TransportController::updatePosition()
{
    if (!playing()) return;
    double position = m_baseShowMs + m_clock.elapsed();
    if (m_sourceRunning) {
        const bool midi = m_project->m_playbackSource == QStringLiteral("midi");
        const double sourcePosition = midi ? m_project->openingDurationMs() + m_synth->positionMs()
            : showMsAtAudioMs(m_player->position());
        // Ignore stale asynchronous seek reports. Only a clock close to the new
        // target can become authoritative; a previous seek cannot snap us back.
        const bool ended = midi ? sourcePosition >= showMsAtTick(m_project->m_music.durationTick)
            : m_player->mediaStatus() == QMediaPlayer::EndOfMedia || m_player->error() != QMediaPlayer::NoError;
        if (ended) {
            m_sourceRunning = false; m_synth->pause(); m_player->pause();
        } else if (sourcePosition > m_showMs && qAbs(sourcePosition - position) < 250.0) {
            position = sourcePosition; m_baseShowMs = position; m_clock.restart();
        }
    }
    const double loopA = showMsAtTick(loopStartTick()), loopB = showMsAtTick(loopEndTick());
    if (m_project->m_loopEnabled && loopB > loopA && position >= loopB) { seekMs(loopA); return; }
    const double end = m_playEndTick > 0 ? qMin(showMsAtTick(m_playEndTick), durationMs()) : durationMs();
    if (position >= end) {
        applyShowMs(end);
        m_timer.stop(); m_synth->pause(); m_player->pause(); m_sourceRunning = false;
        m_state = QStringLiteral("stopped"); m_playEndTick = 0; emit stateChanged(); return;
    }
    const bool crossedOpening = m_showMs < m_project->openingDurationMs() && position >= m_project->openingDurationMs();
    const bool crossedAudioStart = audioMsAtShowMs(m_showMs) < 0.0 && audioMsAtShowMs(position) >= 0.0;
    applyShowMs(position);
    if (crossedOpening || crossedAudioStart) syncSource();
}
