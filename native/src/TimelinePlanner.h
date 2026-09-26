#pragma once

#include "MusicDocument.h"

#include <QJsonObject>
#include <QString>
#include <QVariantMap>
#include <QVector>

namespace MarchCraft {

struct TimelineMarker {
    QString id;
    qint64 tick = 0;
    QString name;
    QString type{QStringLiteral("user")};
    QString color{QStringLiteral("#f59e0b")};
    QString notes;

    QJsonObject toJson() const;
    static TimelineMarker fromJson(const QJsonObject &object);
};

struct SetPlanSection {
    QString name;
    QString type;
    qint64 startTick = 0;
    qint64 endTick = 0;
};

struct ExistingSetTiming {
    int index = -1;
    QString number;
    qint64 tick = 0;
    int counts = 0;
};

struct SetPlanOptions {
    QString density{QStringLiteral("balanced")};
    QString priority{QStringLiteral("balanced")};
    QVector<int> preferredCounts{8, 12, 16, 24, 32};
    int maximumSets = 112;
};

struct SetPlanCandidate {
    QString id;
    qint64 tick = 0;
    qint64 sourceTick = -1;
    QString kind;
    QString action{QStringLiteral("add")};
    QString title;
    QString reason;
    QString color{QStringLiteral("#8b5cf6")};
    double confidence = 0.0;
    double timeMs = 0.0;
    int measure = 0;
    int beat = 0;
    int countsFromPrevious = 0;
    int existingSetIndex = -1;
    bool accepted = false;
    bool manual = false;

    QVariantMap toVariant() const;
};

class SetPlanAnalyzer final
{
public:
    static QVector<SetPlanCandidate> analyze(const MusicDocument &music,
                                             const QVector<TimelineMarker> &markers,
                                             const QVector<SetPlanSection> &sections,
                                             const QVector<ExistingSetTiming> &existingSets,
                                             const SetPlanOptions &options);
};

} // namespace MarchCraft
