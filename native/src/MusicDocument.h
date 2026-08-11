#pragma once

#include "DrillTypes.h"

#include <QJsonObject>
#include <QString>
#include <QVector>

namespace MarchCraft {

struct MusicTempoEvent {
    qint64 tick = 0;
    double bpm = 120.0;
};

struct MusicMeterEvent {
    qint64 tick = 0;
    int numerator = 4;
    int denominator = 4;
    qint64 pulseTicks = TicksPerQuarter;
    QString grouping{QStringLiteral("4")};
};

struct MusicTrack {
    int index = 0;
    QString name;
    int noteCount = 0;
    int channel = -1;
    int program = -1;
    bool selected = false;
    bool muted = false;
    bool solo = false;
    double volume = 1.0;
};

struct MidiPlaybackEvent {
    qint64 tick = 0;
    int track = 0;
    quint8 status = 0;
    quint8 data1 = 0;
    quint8 data2 = 0;
};

struct MusicMeasure {
    int index = 0;
    int displayNumber = 1;
    qint64 startTick = 0;
    qint64 endTick = 0;
    int numerator = 4;
    int denominator = 4;
    qint64 pulseTicks = TicksPerQuarter;
    int counts = 4;
    int noteCount = 0;
    bool partial = false;
};

struct MusicKeyEvent {
    qint64 tick = 0;
    int sharps = 0;
    bool minor = false;
};

struct MusicMarker {
    qint64 tick = 0;
    QString text;
    QString kind;
};

struct AudioAnchor {
    double audioMs = 0.0;
    qint64 musicTick = 0;
};

struct MusicDocument {
    QString sourceType;
    QString sourcePath;
    QString sourceHash;
    int sourcePpq = 0;
    qint64 durationTick = 0;
    double durationMs = 0.0;
    int firstMeasureNumber = 1;
    QVector<MusicTempoEvent> tempos;
    QVector<MusicMeterEvent> meters;
    QVector<MusicTrack> tracks;
    QVector<MusicMeasure> measures;
    QVector<MusicKeyEvent> keys;
    QVector<MusicMarker> markers;
    QVector<MidiPlaybackEvent> playbackEvents;
    QVector<AudioAnchor> audioAnchors;
    QStringList diagnostics;

    bool loaded() const { return !sourceType.isEmpty() && durationTick > 0; }
    void clear();
    void rebuildMeasures(const QVector<qint64> &noteOnTicks = {});
    double millisecondsAt(qint64 tick) const;
    qint64 tickAtMilliseconds(double milliseconds) const;
    QJsonObject toJson() const;
    static MusicDocument fromJson(const QJsonObject &object);
};

struct MidiImportResult {
    bool ok = false;
    MusicDocument document;
    QString error;
};

MidiImportResult parseMidiFile(const QString &path);

} // namespace MarchCraft
