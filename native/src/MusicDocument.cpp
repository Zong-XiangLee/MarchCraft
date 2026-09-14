#include "MusicDocument.h"

#include <QCryptographicHash>
#include <QFile>
#include <QJsonArray>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <limits>

namespace MarchCraft {
namespace {

quint16 read16(const QByteArray &data, qsizetype offset)
{
    return (quint16(quint8(data[offset])) << 8) | quint8(data[offset + 1]);
}

quint32 read32(const QByteArray &data, qsizetype offset)
{
    return (quint32(quint8(data[offset])) << 24) | (quint32(quint8(data[offset + 1])) << 16)
        | (quint32(quint8(data[offset + 2])) << 8) | quint8(data[offset + 3]);
}

bool readVlq(const QByteArray &data, qsizetype *offset, quint32 *value, qsizetype end)
{
    quint32 result = 0;
    for (int byte = 0; byte < 4; ++byte) {
        if (*offset >= end) return false;
        const quint8 current = quint8(data[(*offset)++]);
        result = (result << 7) | (current & 0x7f);
        if ((current & 0x80) == 0) { *value = result; return true; }
    }
    return false;
}

QString midiText(QByteArray value)
{
    while (!value.isEmpty() && value.endsWith('\0')) value.chop(1);
    QString text = QString::fromUtf8(value);
    if (text.contains(QChar::ReplacementCharacter)) text = QString::fromLatin1(value);
    return text.trimmed();
}

qint64 scaledTick(qint64 tick, int ppq)
{
    return qRound64(double(tick) * TicksPerQuarter / qMax(1, ppq));
}

qint64 defaultPulseTicks(int numerator, int denominator)
{
    if (denominator == 8 && numerator >= 6 && numerator % 3 == 0)
        return TicksPerQuarter * 3 / 2;
    return qMax<qint64>(1, TicksPerQuarter * 4 / qMax(1, denominator));
}

QString defaultGrouping(int numerator, int denominator)
{
    if (denominator == 8 && numerator >= 6 && numerator % 3 == 0) {
        QStringList groups;
        for (int i = 0; i < numerator / 3; ++i) groups.push_back(QStringLiteral("3"));
        return groups.join(QLatin1Char('+'));
    }
    return QString::number(numerator);
}

template<typename T, typename TickGetter>
void sortAndDedupe(QVector<T> &events, TickGetter tickGetter)
{
    std::stable_sort(events.begin(), events.end(), [&](const T &a, const T &b) {
        return tickGetter(a) < tickGetter(b);
    });
    QVector<T> result;
    for (const auto &event : std::as_const(events)) {
        if (!result.isEmpty() && tickGetter(result.last()) == tickGetter(event)) result.last() = event;
        else result.push_back(event);
    }
    events = std::move(result);
}

QJsonArray anchorsToJson(const QVector<AudioAnchor> &anchors)
{
    QJsonArray result;
    for (const auto &anchor : anchors)
        result.push_back(QJsonObject{{QStringLiteral("audioMs"), anchor.audioMs},
                                    {QStringLiteral("musicTick"), anchor.musicTick}});
    return result;
}

} // namespace

void MusicDocument::clear()
{
    *this = MusicDocument{};
}

void MusicDocument::rebuildMeasures(const QVector<qint64> &noteOnTicks)
{
    measures.clear();
    if (durationTick <= 0) return;
    if (meters.isEmpty()) meters.push_back({});
    sortAndDedupe(meters, [](const MusicMeterEvent &event) { return event.tick; });
    qint64 cursor = 0;
    int meterIndex = 0;
    int measureIndex = 0;
    while (cursor < durationTick) {
        while (meterIndex + 1 < meters.size() && meters[meterIndex + 1].tick <= cursor) ++meterIndex;
        const auto &meter = meters[meterIndex];
        const qint64 nominal = qMax<qint64>(1, qRound64(double(TicksPerQuarter) * 4.0
                                                        * meter.numerator / meter.denominator));
        const qint64 nextMeter = meterIndex + 1 < meters.size()
            ? meters[meterIndex + 1].tick : std::numeric_limits<qint64>::max();
        const qint64 end = qMin(durationTick, qMin(cursor + nominal, nextMeter));
        MusicMeasure measure;
        measure.index = measureIndex;
        measure.displayNumber = firstMeasureNumber + measureIndex;
        measure.startTick = cursor;
        measure.endTick = qMax(cursor + 1, end);
        measure.numerator = meter.numerator;
        measure.denominator = meter.denominator;
        measure.pulseTicks = meter.pulseTicks;
        measure.counts = qMax(1, qRound(double(measure.endTick - cursor) / qMax<qint64>(1, meter.pulseTicks)));
        measure.partial = measure.endTick - cursor != nominal;
        measure.noteCount = static_cast<int>(std::count_if(noteOnTicks.cbegin(), noteOnTicks.cend(),
            [&](qint64 tick) { return tick >= cursor && tick < measure.endTick; }));
        measures.push_back(measure);
        cursor = measure.endTick;
        ++measureIndex;
    }
}

double MusicDocument::millisecondsAt(qint64 tick) const
{
    tick = qBound<qint64>(0, tick, durationTick);
    double result = 0.0;
    qint64 cursor = 0;
    double bpm = tempos.isEmpty() ? 120.0 : tempos.first().bpm;
    for (const auto &tempo : tempos) {
        if (tempo.tick > tick) break;
        if (tempo.tick > cursor)
            result += (tempo.tick - cursor) * 60000.0 / (TicksPerQuarter * bpm);
        cursor = qMax(cursor, tempo.tick);
        bpm = tempo.bpm;
    }
    if (tick > cursor) result += (tick - cursor) * 60000.0 / (TicksPerQuarter * bpm);
    return result;
}

qint64 MusicDocument::tickAtMilliseconds(double milliseconds) const
{
    milliseconds = qMax(0.0, milliseconds);
    qint64 cursor = 0;
    double elapsed = 0.0;
    double bpm = tempos.isEmpty() ? 120.0 : tempos.first().bpm;
    for (const auto &tempo : tempos) {
        if (tempo.tick < cursor) { bpm = tempo.bpm; continue; }
        const double span = (tempo.tick - cursor) * 60000.0 / (TicksPerQuarter * bpm);
        if (elapsed + span >= milliseconds)
            return qBound<qint64>(0, cursor + qRound64((milliseconds - elapsed) * TicksPerQuarter * bpm / 60000.0), durationTick);
        elapsed += span; cursor = tempo.tick; bpm = tempo.bpm;
    }
    return qBound<qint64>(0, cursor + qRound64((milliseconds - elapsed) * TicksPerQuarter * bpm / 60000.0), durationTick);
}

QJsonObject MusicDocument::toJson() const
{
    QJsonArray tempoArray, meterArray, trackArray, measureArray, keyArray, markerArray;
    for (const auto &tempo : tempos) tempoArray.push_back(QJsonObject{{QStringLiteral("tick"), tempo.tick}, {QStringLiteral("bpm"), tempo.bpm}});
    for (const auto &meter : meters) meterArray.push_back(QJsonObject{{QStringLiteral("tick"), meter.tick},
        {QStringLiteral("numerator"), meter.numerator}, {QStringLiteral("denominator"), meter.denominator},
        {QStringLiteral("pulseTicks"), meter.pulseTicks}, {QStringLiteral("grouping"), meter.grouping}});
    for (const auto &track : tracks) trackArray.push_back(QJsonObject{{QStringLiteral("index"), track.index},
        {QStringLiteral("name"), track.name}, {QStringLiteral("noteCount"), track.noteCount},
        {QStringLiteral("channel"), track.channel}, {QStringLiteral("program"), track.program},
        {QStringLiteral("selected"), track.selected}, {QStringLiteral("muted"), track.muted},
        {QStringLiteral("solo"), track.solo}, {QStringLiteral("volume"), track.volume}});
    for (const auto &measure : measures) measureArray.push_back(QJsonObject{{QStringLiteral("index"), measure.index},
        {QStringLiteral("displayNumber"), measure.displayNumber}, {QStringLiteral("startTick"), measure.startTick},
        {QStringLiteral("endTick"), measure.endTick}, {QStringLiteral("numerator"), measure.numerator},
        {QStringLiteral("denominator"), measure.denominator}, {QStringLiteral("pulseTicks"), measure.pulseTicks},
        {QStringLiteral("counts"), measure.counts}, {QStringLiteral("noteCount"), measure.noteCount},
        {QStringLiteral("partial"), measure.partial}});
    for (const auto &key : keys) keyArray.push_back(QJsonObject{{QStringLiteral("tick"), key.tick},
        {QStringLiteral("sharps"), key.sharps}, {QStringLiteral("minor"), key.minor}});
    for (const auto &marker : markers) markerArray.push_back(QJsonObject{{QStringLiteral("tick"), marker.tick},
        {QStringLiteral("text"), marker.text}, {QStringLiteral("kind"), marker.kind}});
    return {{QStringLiteral("sourceType"), sourceType}, {QStringLiteral("sourcePath"), sourcePath},
            {QStringLiteral("sourceHash"), sourceHash}, {QStringLiteral("sourcePpq"), sourcePpq},
            {QStringLiteral("durationTick"), durationTick}, {QStringLiteral("durationMs"), durationMs},
            {QStringLiteral("firstMeasureNumber"), firstMeasureNumber}, {QStringLiteral("tempos"), tempoArray},
            {QStringLiteral("meters"), meterArray}, {QStringLiteral("tracks"), trackArray},
            {QStringLiteral("measures"), measureArray}, {QStringLiteral("keys"), keyArray},
            {QStringLiteral("markers"), markerArray}, {QStringLiteral("audioAnchors"), anchorsToJson(audioAnchors)},
            {QStringLiteral("diagnostics"), QJsonArray::fromStringList(diagnostics)}};
}

MusicDocument MusicDocument::fromJson(const QJsonObject &object)
{
    MusicDocument document;
    document.sourceType = object.value(QStringLiteral("sourceType")).toString();
    document.sourcePath = object.value(QStringLiteral("sourcePath")).toString();
    document.sourceHash = object.value(QStringLiteral("sourceHash")).toString();
    document.sourcePpq = object.value(QStringLiteral("sourcePpq")).toInt();
    document.durationTick = object.value(QStringLiteral("durationTick")).toVariant().toLongLong();
    document.durationMs = object.value(QStringLiteral("durationMs")).toDouble();
    document.firstMeasureNumber = object.value(QStringLiteral("firstMeasureNumber")).toInt(1);
    for (const auto &value : object.value(QStringLiteral("tempos")).toArray()) { const auto o = value.toObject();
        document.tempos.push_back({o.value(QStringLiteral("tick")).toVariant().toLongLong(), o.value(QStringLiteral("bpm")).toDouble(120.0)}); }
    for (const auto &value : object.value(QStringLiteral("meters")).toArray()) { const auto o = value.toObject();
        document.meters.push_back({o.value(QStringLiteral("tick")).toVariant().toLongLong(),
            o.value(QStringLiteral("numerator")).toInt(4), o.value(QStringLiteral("denominator")).toInt(4),
            o.value(QStringLiteral("pulseTicks")).toVariant().toLongLong(), o.value(QStringLiteral("grouping")).toString()}); }
    for (const auto &value : object.value(QStringLiteral("tracks")).toArray()) { const auto o = value.toObject(); MusicTrack track;
        track.index=o.value(QStringLiteral("index")).toInt(); track.name=o.value(QStringLiteral("name")).toString();
        track.noteCount=o.value(QStringLiteral("noteCount")).toInt(); track.channel=o.value(QStringLiteral("channel")).toInt(-1);
        track.program=o.value(QStringLiteral("program")).toInt(-1); track.selected=o.value(QStringLiteral("selected")).toBool();
        track.muted=o.value(QStringLiteral("muted")).toBool(); track.solo=o.value(QStringLiteral("solo")).toBool();
        track.volume=qBound(0.0,o.value(QStringLiteral("volume")).toDouble(1.0),1.0); document.tracks.push_back(track); }
    for (const auto &value : object.value(QStringLiteral("measures")).toArray()) { const auto o=value.toObject(); MusicMeasure measure;
        measure.index=o.value(QStringLiteral("index")).toInt(); measure.displayNumber=o.value(QStringLiteral("displayNumber")).toInt(measure.index+1);
        measure.startTick=o.value(QStringLiteral("startTick")).toVariant().toLongLong(); measure.endTick=o.value(QStringLiteral("endTick")).toVariant().toLongLong();
        measure.numerator=o.value(QStringLiteral("numerator")).toInt(4); measure.denominator=o.value(QStringLiteral("denominator")).toInt(4);
        measure.pulseTicks=o.value(QStringLiteral("pulseTicks")).toVariant().toLongLong(); measure.counts=o.value(QStringLiteral("counts")).toInt(4);
        measure.noteCount=o.value(QStringLiteral("noteCount")).toInt(); measure.partial=o.value(QStringLiteral("partial")).toBool(); document.measures.push_back(measure); }
    for (const auto &value : object.value(QStringLiteral("keys")).toArray()) { const auto o=value.toObject(); document.keys.push_back({
        o.value(QStringLiteral("tick")).toVariant().toLongLong(), o.value(QStringLiteral("sharps")).toInt(), o.value(QStringLiteral("minor")).toBool()}); }
    for (const auto &value : object.value(QStringLiteral("markers")).toArray()) { const auto o=value.toObject(); document.markers.push_back({
        o.value(QStringLiteral("tick")).toVariant().toLongLong(), o.value(QStringLiteral("text")).toString(), o.value(QStringLiteral("kind")).toString()}); }
    for (const auto &value : object.value(QStringLiteral("audioAnchors")).toArray()) { const auto o=value.toObject(); document.audioAnchors.push_back({
        o.value(QStringLiteral("audioMs")).toDouble(), o.value(QStringLiteral("musicTick")).toVariant().toLongLong()}); }
    for (const auto &value : object.value(QStringLiteral("diagnostics")).toArray()) document.diagnostics.push_back(value.toString());
    return document;
}

MidiImportResult parseMidiFile(const QString &path)
{
    MidiImportResult result;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) { result.error = QStringLiteral("Could not open MIDI file"); return result; }
    const QByteArray data = file.readAll();
    if (data.size() < 14 || data.first(4) != QByteArrayLiteral("MThd")) { result.error = QStringLiteral("Invalid MIDI header"); return result; }
    const quint32 headerLength = read32(data, 4);
    if (headerLength < 6 || qsizetype(headerLength) > data.size() - 8) { result.error = QStringLiteral("Invalid MIDI header length"); return result; }
    const int format = read16(data, 8), trackCount = read16(data, 10), division = read16(data, 12);
    if (format > 1) { result.error = QStringLiteral("MIDI format 2 is not supported"); return result; }
    if (division & 0x8000) { result.error = QStringLiteral("SMPTE-timed MIDI files are not supported"); return result; }
    if (trackCount <= 0 || division <= 0) { result.error = QStringLiteral("MIDI file has no readable tracks"); return result; }

    MusicDocument document;
    document.sourceType = QStringLiteral("midi"); document.sourcePath = path; document.sourcePpq = division;
    document.sourceHash = QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
    QVector<qint64> noteTicks;
    qsizetype fileOffset = 8 + qsizetype(headerLength);
    qint64 maximumTick = 0;
    for (int trackIndex = 0; trackIndex < trackCount; ++trackIndex) {
        if (fileOffset + 8 > data.size() || data.mid(fileOffset, 4) != QByteArrayLiteral("MTrk")) {
            result.error = QStringLiteral("Missing MIDI track %1").arg(trackIndex + 1); return result;
        }
        const quint32 length = read32(data, fileOffset + 4);
        const qsizetype trackStart = fileOffset + 8, trackEnd = trackStart + length;
        if (trackEnd > data.size()) { result.error = QStringLiteral("Truncated MIDI track %1").arg(trackIndex + 1); return result; }
        MusicTrack track; track.index = trackIndex; track.name = QStringLiteral("Track %1").arg(trackIndex + 1);
        qsizetype offset = trackStart; qint64 tick = 0; quint8 running = 0;
        while (offset < trackEnd) {
            quint32 delta = 0;
            if (!readVlq(data, &offset, &delta, trackEnd)) { result.error = QStringLiteral("Invalid MIDI delta time"); return result; }
            tick += delta; maximumTick = qMax(maximumTick, tick);
            if (offset >= trackEnd) { result.error = QStringLiteral("Truncated MIDI event"); return result; }
            quint8 status = quint8(data[offset]);
            if (status < 0x80) {
                if (running == 0) { result.error = QStringLiteral("Invalid MIDI running status"); return result; }
                status = running;
            } else {
                ++offset;
                if (status < 0xf0) running = status;
            }
            if (status == 0xff) {
                if (offset >= trackEnd) { result.error = QStringLiteral("Truncated MIDI event"); return result; }
                const quint8 type = quint8(data[offset++]); quint32 metaLength = 0;
                if (!readVlq(data, &offset, &metaLength, trackEnd) || offset + metaLength > trackEnd) { result.error = QStringLiteral("Invalid MIDI meta event"); return result; }
                const QByteArray payload = data.mid(offset, metaLength); offset += metaLength;
                const qint64 internalTick = scaledTick(tick, division);
                if (type == 0x03) track.name = midiText(payload);
                else if (type == 0x51 && payload.size() == 3) {
                    const int micros = (quint8(payload[0]) << 16) | (quint8(payload[1]) << 8) | quint8(payload[2]);
                    if (micros > 0) document.tempos.push_back({internalTick, 60000000.0 / micros});
                } else if (type == 0x58 && payload.size() >= 2) {
                    const int numerator = qMax(1, int(quint8(payload[0])));
                    const int denominator = 1 << qMin(7, int(quint8(payload[1])));
                    document.meters.push_back({internalTick, numerator, denominator,
                        defaultPulseTicks(numerator, denominator), defaultGrouping(numerator, denominator)});
                } else if (type == 0x59 && payload.size() >= 2) {
                    document.keys.push_back({internalTick, int(qint8(payload[0])), payload[1] != 0});
                } else if (type == 0x06 || type == 0x07 || type == 0x05) {
                    document.markers.push_back({internalTick, midiText(payload),
                        type == 0x05 ? QStringLiteral("lyric") : type == 0x06 ? QStringLiteral("marker") : QStringLiteral("cue")});
                }
                if (type == 0x2f) break;
            } else if (status == 0xf0 || status == 0xf7) {
                quint32 sysexLength = 0;
                if (!readVlq(data, &offset, &sysexLength, trackEnd) || offset + sysexLength > trackEnd) { result.error = QStringLiteral("Invalid MIDI SysEx event"); return result; }
                offset += sysexLength;
            } else {
                if (status >= 0xf0) { result.error = QStringLiteral("Unsupported MIDI status"); return result; }
                const quint8 kind = status & 0xf0, channel = status & 0x0f;
                const int bytes = (kind == 0xc0 || kind == 0xd0) ? 1 : 2;
                if (offset + bytes > trackEnd) { result.error = QStringLiteral("Truncated MIDI channel event"); return result; }
                const quint8 first = quint8(data[offset]), second = bytes == 2 ? quint8(data[offset + 1]) : 0;
                if (first >= 0x80 || second >= 0x80) { result.error = QStringLiteral("Invalid MIDI channel data"); return result; }
                offset += bytes;
                if (track.channel < 0) track.channel = channel;
                if (kind == 0xc0) track.program = first;
                if (kind == 0x90 && second > 0) {
                    ++track.noteCount; noteTicks.push_back(scaledTick(tick, division));
                }
                document.playbackEvents.push_back({scaledTick(tick, division), trackIndex,
                    status, first, second});
            }
        }
        document.tracks.push_back(track);
        fileOffset = trackEnd;
    }
    document.durationTick = qMax<qint64>(1, scaledTick(maximumTick, division));
    if (document.tempos.isEmpty()) { document.tempos.push_back({0, 120.0}); document.diagnostics.push_back(QStringLiteral("No MIDI tempo event; using 120 BPM")); }
    if (document.meters.isEmpty()) { document.meters.push_back({}); document.diagnostics.push_back(QStringLiteral("No MIDI time signature; using 4/4")); }
    sortAndDedupe(document.tempos, [](const MusicTempoEvent &event) { return event.tick; });
    sortAndDedupe(document.meters, [](const MusicMeterEvent &event) { return event.tick; });
    sortAndDedupe(document.keys, [](const MusicKeyEvent &event) { return event.tick; });
    std::stable_sort(document.playbackEvents.begin(), document.playbackEvents.end(),
        [](const MidiPlaybackEvent &a, const MidiPlaybackEvent &b) {
            if (a.tick != b.tick) return a.tick < b.tick;
            return a.track < b.track;
        });
    if (document.tempos.first().tick > 0) document.tempos.prepend({0, 120.0});
    if (document.meters.first().tick > 0) document.meters.prepend({});
    int selected = 0;
    for (auto &track : document.tracks) if (track.noteCount > 0 && selected < 4) { track.selected = true; ++selected; }
    document.rebuildMeasures(noteTicks);
    document.durationMs = document.millisecondsAt(document.durationTick);
    if (document.markers.isEmpty()) document.diagnostics.push_back(QStringLiteral("No rehearsal markers; select measures to create drill sets"));
    result.ok = true; result.document = std::move(document); return result;
}

} // namespace MarchCraft
