#include "TransportController.h"

#include "DrillProject.h"
#include "MidiSynthEngine.h"

#include <QAudioOutput>
#include <QMediaPlayer>
#include <QUrl>

TransportController::TransportController(DrillProject *project, QObject *parent)
    : QObject(parent), m_project(project), m_synth(new MidiSynthEngine(this)),
      m_player(new QMediaPlayer(this)), m_audioOutput(new QAudioOutput(this))
{
    m_player->setAudioOutput(m_audioOutput); m_timer.setInterval(16);
    connect(&m_timer, &QTimer::timeout, this, &TransportController::updatePosition);
    connect(m_synth, &MidiSynthEngine::statusChanged, this, &TransportController::audioStatusChanged);
    connect(project, &DrillProject::musicChanged, this, [this] { refreshMusic(); });
    connect(project, &DrillProject::transportSettingsChanged, this, [this] {
        m_synth->setGain(m_project->m_midiMasterVolume); syncSource(); emit audioStatusChanged();
    });
    connect(project, &DrillProject::setRangeChanged, this, [this] {
        if (!playing()) {
            const int selected=m_project->m_currentSet; seekTick(setTick(selected));
            m_project->setCurrentSetIndex(selected); m_project->setPlaybackActive(false);
            m_project->setPlayhead(1.0);
        }
        emit loopChanged();
    });
    refreshMusic();
}

qint64 TransportController::durationTick() const
{
    if (m_project->m_music.loaded()) return m_project->m_music.durationTick;
    return m_project->m_sets.isEmpty() ? 0 : m_project->m_sets.last().startTick;
}
double TransportController::currentMs() const { return m_openingHoldActive
    ? m_openingHoldElapsedMs + (m_clock.isValid() && playing() ? m_clock.elapsed() : 0.0)
    : m_project->openingDurationMs() + musicMsAt(m_tick); }
double TransportController::durationMs() const { return m_project->openingDurationMs() + musicMsAt(durationTick()); }
double TransportController::normalizedPosition() const { return durationMs() > 0 ? qBound(0.0,currentMs()/durationMs(),1.0) : 0.0; }
qint64 TransportController::setTick(int index) const { return index>=0&&index<m_project->m_sets.size()?m_project->m_sets[index].startTick:0; }
qint64 TransportController::loopStartTick() const { return setTick(qMin(m_project->m_selectedSetStart,m_project->m_selectedSetEnd)); }
qint64 TransportController::loopEndTick() const { return setTick(qMax(m_project->m_selectedSetStart,m_project->m_selectedSetEnd)); }
QString TransportController::audioStatus() const { return m_project->m_playbackSource==QStringLiteral("midi")?m_synth->status():m_project->m_playbackSource==QStringLiteral("rehearsal")?QStringLiteral("Rehearsal audio"):QStringLiteral("Muted"); }
bool TransportController::synthAvailable() const { return m_synth->available(); }
int TransportController::underrunCount() const { return m_synth->underruns(); }

double TransportController::musicMsAt(qint64 tick) const
{
    return m_project->m_music.loaded() ? m_project->m_music.millisecondsAt(tick)
        : m_project->millisecondsBetween(0, tick);
}
qint64 TransportController::tickAtMusicMs(double ms) const
{
    if (m_project->m_music.loaded()) return m_project->m_music.tickAtMilliseconds(ms);
    qint64 low=0, high=qMax<qint64>(1,durationTick());
    while(low<high){const qint64 mid=(low+high)/2;if(musicMsAt(mid)<ms)low=mid+1;else high=mid;} return low;
}

void TransportController::refreshMusic()
{
    const bool sourceChanged = m_loadedMusicHash != m_project->m_music.sourceHash;
    if (sourceChanged) { m_synth->load(m_project->m_music); m_loadedMusicHash=m_project->m_music.sourceHash; }
    m_synth->updateTracks(m_project->m_music.tracks);
    m_synth->setGain(m_project->m_midiMasterVolume);
    m_player->setSource(m_project->m_audioSource.isEmpty()?QUrl():QUrl::fromLocalFile(m_project->m_audioSource));
    m_tick=qBound<qint64>(0,m_tick,durationTick());
    if (sourceChanged && playing()) syncSource(); emit positionChanged(); emit audioStatusChanged();
}

void TransportController::syncSource()
{
    if (!playing()) return;
    m_synth->pause(); m_player->pause();
    if (m_project->m_playbackSource==QStringLiteral("midi") && m_synth->available()) { m_synth->seekTick(m_tick); m_synth->play(); }
    else if (m_project->m_playbackSource==QStringLiteral("rehearsal") && !m_project->m_audioSource.isEmpty()) {
        m_player->setPosition(qRound64(m_project->audioMsForMusicTick(m_tick))); m_player->play();
    } else { m_baseMusicMs=musicMsAt(m_tick); m_clock.restart(); }
}

void TransportController::startAt(qint64 tick)
{
    m_openingHoldActive=false; m_openingHoldElapsedMs=0.0;
    applyTick(tick); m_state=QStringLiteral("playing"); m_baseMusicMs=musicMsAt(m_tick); m_clock.restart();
    m_project->setPlaybackActive(true); syncSource(); m_timer.start(); emit stateChanged();
}

void TransportController::startOpeningHold(bool reset)
{
    if(reset)m_openingHoldElapsedMs=0.0;
    m_openingHoldActive=true;m_tick=0;m_playEndTick=durationTick();
    m_synth->pause();m_player->pause();m_project->setCurrentSetIndex(0);
    m_project->setPlaybackActive(true);m_project->setPlayhead(1.0);
    m_state=QStringLiteral("playing");m_clock.restart();m_timer.start();emit stateChanged();emit positionChanged();
}

void TransportController::playPause() { if(playing())pause();else{
    if(m_openingHoldActive){startOpeningHold(false);return;}
    if(m_state==QStringLiteral("stopped"))m_playEndTick=durationTick();
    const int startSet=m_tick>=durationTick()?m_project->m_selectedSetStart:m_project->m_currentSet;
    if(setTick(startSet)==0&&m_project->openingDurationMs()>0.0)startOpeningHold();
    else startAt(m_tick>=durationTick()?setTick(startSet):m_tick);
} }
void TransportController::playFromSelection() { m_playEndTick=durationTick();const int start=qMin(m_project->m_selectedSetStart,m_project->m_selectedSetEnd);if(start==0&&m_project->openingDurationMs()>0.0)startOpeningHold();else startAt(setTick(start)); }
void TransportController::playCurrentTransition() { if(m_project->m_sets.size()<2)return;const int destination=qBound(1,m_project->m_currentSet,m_project->m_sets.size()-1);m_playEndTick=setTick(destination);startAt(setTick(destination-1)); }
void TransportController::pause()
{
    if(!playing())return; updatePosition(); m_timer.stop(); m_synth->pause(); m_player->pause();
    if(m_openingHoldActive){m_openingHoldElapsedMs+=m_clock.elapsed();m_clock.invalidate();}
    m_state=QStringLiteral("paused"); m_project->setPlaybackActive(true); emit stateChanged();
}
void TransportController::stop()
{
    m_timer.stop(); m_synth->pause(); m_player->stop(); m_state=QStringLiteral("stopped");
    m_openingHoldActive=false;m_openingHoldElapsedMs=0.0;
    m_project->setPlaybackActive(true); emit stateChanged(); emit positionChanged();
}
void TransportController::seekTick(qint64 tick)
{
    m_openingHoldActive=false;m_openingHoldElapsedMs=0.0;
    const bool resume=playing(); m_synth->pause(); m_player->pause(); applyTick(tick); m_project->setPlaybackActive(true);
    m_baseMusicMs=musicMsAt(m_tick); m_clock.restart(); if(resume)syncSource();
}
void TransportController::seekNormalized(double position) { seekTick(qRound64(qBound(0.0,position,1.0)*durationTick())); }
void TransportController::firstSet() { seekTick(setTick(0)); m_project->selectSetRange(0,false); }
void TransportController::previousSet() { const int i=qMax(0,m_project->m_currentSet-1);seekTick(setTick(i));m_project->selectSetRange(i,false); }
void TransportController::nextSet() { const int i=qMin(m_project->m_sets.size()-1,m_project->m_currentSet+1);seekTick(setTick(i));m_project->selectSetRange(i,false); }
void TransportController::toggleLoop() { m_project->setLoopEnabled(!m_project->m_loopEnabled); emit loopChanged(); }

void TransportController::applyTick(qint64 tick)
{
    m_tick=qBound<qint64>(0,tick,durationTick()); int destination=0;
    while(destination+1<m_project->m_sets.size() && m_project->m_sets[destination+1].startTick<=m_tick)++destination;
    if(destination+1<m_project->m_sets.size()) {
        const qint64 a=m_project->m_sets[destination].startTick,b=m_project->m_sets[destination+1].startTick;
        m_project->setCurrentSetIndex(destination+1); m_project->setPlayhead(b>a?double(m_tick-a)/double(b-a):0.0);
    } else { m_project->setCurrentSetIndex(destination);m_project->setPlayhead(1.0); }
    emit positionChanged();
}

void TransportController::updatePosition()
{
    if(!playing())return; qint64 tick=m_tick;
    if(m_openingHoldActive){
        const double elapsed=m_openingHoldElapsedMs+m_clock.elapsed();
        if(elapsed<m_project->openingDurationMs()){emit positionChanged();return;}
        m_openingHoldActive=false;m_openingHoldElapsedMs=0.0;startAt(0);return;
    }
    if(m_project->m_playbackSource==QStringLiteral("midi")&&m_synth->available()) tick=tickAtMusicMs(m_synth->positionMs());
    else if(m_project->m_playbackSource==QStringLiteral("rehearsal")&&!m_project->m_audioSource.isEmpty()) tick=m_project->musicTickForAudioMs(m_player->position());
    else tick=tickAtMusicMs(m_baseMusicMs+m_clock.elapsed());
    const qint64 loopA=loopStartTick(),loopB=loopEndTick();
    if(m_project->m_loopEnabled&&loopB>loopA&&tick>=loopB){seekTick(loopA);return;}
    const qint64 end=m_playEndTick>0?qMin(m_playEndTick,durationTick()):durationTick();
    if(tick>=end){applyTick(end);stop();return;} applyTick(tick);
}
