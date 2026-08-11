#pragma once

#include "SceneTypes.h"

#include <QColor>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QPointF>
#include <QString>
#include <QUuid>
#include <QVector>
#include <limits>

namespace MarchCraft {

inline constexpr qint64 TicksPerQuarter = 960;

enum class FormationAssignmentMode {
    RehearsalSafe,
    ShortestTotal,
    PreserveOrder,
    EvenEffort,
    FeatureMove,
    RosterOrder
};

inline QString assignmentModeName(FormationAssignmentMode mode)
{
    switch (mode) {
    case FormationAssignmentMode::ShortestTotal: return QStringLiteral("shortest");
    case FormationAssignmentMode::PreserveOrder: return QStringLiteral("preserveOrder");
    case FormationAssignmentMode::EvenEffort: return QStringLiteral("evenEffort");
    case FormationAssignmentMode::FeatureMove: return QStringLiteral("featureMove");
    case FormationAssignmentMode::RosterOrder: return QStringLiteral("rosterOrder");
    default: return QStringLiteral("rehearsalSafe");
    }
}

inline FormationAssignmentMode assignmentModeFromName(QString value)
{
    value = value.trimmed();
    if (value == QStringLiteral("shortest")) return FormationAssignmentMode::ShortestTotal;
    if (value == QStringLiteral("preserveOrder")) return FormationAssignmentMode::PreserveOrder;
    if (value == QStringLiteral("evenEffort")) return FormationAssignmentMode::EvenEffort;
    if (value == QStringLiteral("featureMove") || value == QStringLiteral("expressive"))
        return FormationAssignmentMode::FeatureMove;
    if (value == QStringLiteral("rosterOrder") || value == QStringLiteral("balanced"))
        return FormationAssignmentMode::RosterOrder;
    return FormationAssignmentMode::RehearsalSafe;
}

struct CapabilityProfile {
    QString name{QStringLiteral("intermediate")};
    double maximumStepsPerCount = 1.25;
    double collisionClearance = 1.5;
    double directionChangeDegrees = 120.0;

    QJsonObject toJson() const {
        return {{QStringLiteral("name"), name},
                {QStringLiteral("maximumStepsPerCount"), maximumStepsPerCount},
                {QStringLiteral("collisionClearance"), collisionClearance},
                {QStringLiteral("directionChangeDegrees"), directionChangeDegrees}};
    }

    static CapabilityProfile preset(QString name) {
        CapabilityProfile profile;
        if (name == QStringLiteral("beginner")) {
            profile.name = name; profile.maximumStepsPerCount = 1.0;
            profile.collisionClearance = 2.0; profile.directionChangeDegrees = 90.0;
        } else if (name == QStringLiteral("advanced")) {
            profile.name = name; profile.maximumStepsPerCount = 1.5;
            profile.collisionClearance = 1.25; profile.directionChangeDegrees = 150.0;
        } else if (name == QStringLiteral("custom")) profile.name = name;
        return profile;
    }

    static CapabilityProfile fromJson(const QJsonObject &object) {
        CapabilityProfile profile = preset(object.value(QStringLiteral("name")).toString(QStringLiteral("intermediate")));
        profile.maximumStepsPerCount = qBound(0.25, object.value(QStringLiteral("maximumStepsPerCount")).toDouble(profile.maximumStepsPerCount), 4.0);
        profile.collisionClearance = qBound(0.25, object.value(QStringLiteral("collisionClearance")).toDouble(profile.collisionClearance), 8.0);
        profile.directionChangeDegrees = qBound(15.0, object.value(QStringLiteral("directionChangeDegrees")).toDouble(profile.directionChangeDegrees), 180.0);
        return profile;
    }
};

struct MeterRegion {
    qint64 startTick = 0;
    qint64 endTick = std::numeric_limits<qint64>::max();
    int numerator = 4;
    int denominator = 4;
    qint64 pulseTicks = TicksPerQuarter;
    QString grouping{QStringLiteral("4")};

    QJsonObject toJson() const { return {{QStringLiteral("startTick"), startTick},
        {QStringLiteral("endTick"), endTick}, {QStringLiteral("numerator"), numerator},
        {QStringLiteral("denominator"), denominator}, {QStringLiteral("pulseTicks"), pulseTicks},
        {QStringLiteral("grouping"), grouping}}; }
    static MeterRegion fromJson(const QJsonObject &o) { MeterRegion r;
        r.startTick = o.value(QStringLiteral("startTick")).toVariant().toLongLong();
        r.endTick = o.value(QStringLiteral("endTick")).toVariant().toLongLong();
        if (r.endTick <= r.startTick) r.endTick = std::numeric_limits<qint64>::max();
        r.numerator = qMax(1, o.value(QStringLiteral("numerator")).toInt(4));
        r.denominator = qMax(1, o.value(QStringLiteral("denominator")).toInt(4));
        r.pulseTicks = qMax<qint64>(1, o.value(QStringLiteral("pulseTicks")).toVariant().toLongLong());
        r.grouping = o.value(QStringLiteral("grouping")).toString(); return r; }
};

struct TempoRegion {
    qint64 startTick = 0;
    qint64 endTick = std::numeric_limits<qint64>::max();
    double startBpm = 120.0;
    double endBpm = 120.0;
    QString name{QStringLiteral("Tempo")};

    QJsonObject toJson() const { return {{QStringLiteral("startTick"), startTick},
        {QStringLiteral("endTick"), endTick}, {QStringLiteral("startBpm"), startBpm},
        {QStringLiteral("endBpm"), endBpm}, {QStringLiteral("name"), name}}; }
    static TempoRegion fromJson(const QJsonObject &o) { TempoRegion r;
        r.startTick = o.value(QStringLiteral("startTick")).toVariant().toLongLong();
        r.endTick = o.value(QStringLiteral("endTick")).toVariant().toLongLong();
        if (r.endTick <= r.startTick) r.endTick = std::numeric_limits<qint64>::max();
        r.startBpm = qBound(20.0, o.value(QStringLiteral("startBpm")).toDouble(120.0), 400.0);
        r.endBpm = qBound(20.0, o.value(QStringLiteral("endBpm")).toDouble(r.startBpm), 400.0);
        r.name = o.value(QStringLiteral("name")).toString(QStringLiteral("Tempo")); return r; }
};

struct Placement {
    QPointF position{80.0, 28.0};
    double facing = 0.0;
    QString pathType{QStringLiteral("direct")};
    QVector<QPointF> pathPoints;

    QJsonObject toJson() const
    {
        QJsonArray points;
        for (const auto &point : pathPoints)
            points.push_back(QJsonObject{{QStringLiteral("x"), point.x()}, {QStringLiteral("y"), point.y()}});
        return {{QStringLiteral("x"), position.x()},
                {QStringLiteral("y"), position.y()},
                {QStringLiteral("facing"), facing},
                {QStringLiteral("pathType"), pathType}, {QStringLiteral("pathPoints"), points}};
    }

    static Placement fromJson(const QJsonObject &object)
    {
        Placement result;
        result.position.setX(object.value(QStringLiteral("x")).toDouble(80.0));
        result.position.setY(object.value(QStringLiteral("y")).toDouble(28.0));
        result.facing = object.value(QStringLiteral("facing")).toDouble();
        result.pathType = object.value(QStringLiteral("pathType")).toString(QStringLiteral("direct"));
        for (const auto &value : object.value(QStringLiteral("pathPoints")).toArray()) {
            const auto p = value.toObject();
            result.pathPoints.push_back({p.value(QStringLiteral("x")).toDouble(), p.value(QStringLiteral("y")).toDouble()});
        }
        return result;
    }
};

struct FormationShape {
    QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QString type{QStringLiteral("line")};
    QVector<QPointF> points;
    QVector<QString> performerIds;
    bool endpointLock = true;
    bool reversed = false;
    QPointF anchor{80.0, 42.0};
    double width = 32.0;
    double height = 24.0;
    double rotation = 0.0;
    bool closed = false;
    QJsonObject parameters;
    QString groupId;

    QJsonObject toJson() const {
        QJsonArray geometry, members;
        for (const auto &p : points) geometry.push_back(QJsonObject{{QStringLiteral("x"), p.x()}, {QStringLiteral("y"), p.y()}});
        for (const auto &id : performerIds) members.push_back(id);
        return {{QStringLiteral("id"), id}, {QStringLiteral("type"), type},
                {QStringLiteral("points"), geometry}, {QStringLiteral("performerIds"), members},
                {QStringLiteral("endpointLock"), endpointLock}, {QStringLiteral("reversed"), reversed},
                {QStringLiteral("anchorX"), anchor.x()}, {QStringLiteral("anchorY"), anchor.y()},
                {QStringLiteral("width"), width}, {QStringLiteral("height"), height},
                {QStringLiteral("rotation"), rotation}, {QStringLiteral("closed"), closed},
                {QStringLiteral("parameters"), parameters}, {QStringLiteral("groupId"), groupId}};
    }
    static FormationShape fromJson(const QJsonObject &o) { FormationShape s;
        s.id = o.value(QStringLiteral("id")).toString(s.id);
        s.type = o.value(QStringLiteral("type")).toString(QStringLiteral("line"));
        for (const auto &v : o.value(QStringLiteral("points")).toArray()) { const auto p = v.toObject();
            s.points.push_back({p.value(QStringLiteral("x")).toDouble(), p.value(QStringLiteral("y")).toDouble()}); }
        for (const auto &v : o.value(QStringLiteral("performerIds")).toArray()) s.performerIds.push_back(v.toString());
        s.endpointLock = o.value(QStringLiteral("endpointLock")).toBool(true);
        s.reversed = o.value(QStringLiteral("reversed")).toBool();
        s.anchor = {o.value(QStringLiteral("anchorX")).toDouble(80.0),
                    o.value(QStringLiteral("anchorY")).toDouble(42.0)};
        s.width = o.value(QStringLiteral("width")).toDouble(32.0);
        s.height = o.value(QStringLiteral("height")).toDouble(24.0);
        s.rotation = o.value(QStringLiteral("rotation")).toDouble();
        s.closed = o.value(QStringLiteral("closed")).toBool();
        s.parameters = o.value(QStringLiteral("parameters")).toObject();
        s.groupId = o.value(QStringLiteral("groupId")).toString(); return s; }
};

struct PerformerGroup {
    QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QString name;
    QVector<QString> performerIds;

    QJsonObject toJson() const {
        QJsonArray members;
        for (const auto &performerId : performerIds) members.push_back(performerId);
        return {{QStringLiteral("id"), id}, {QStringLiteral("name"), name},
                {QStringLiteral("performerIds"), members}};
    }
    static PerformerGroup fromJson(const QJsonObject &object) { PerformerGroup group;
        group.id = object.value(QStringLiteral("id")).toString(group.id);
        group.name = object.value(QStringLiteral("name")).toString();
        for (const auto &member : object.value(QStringLiteral("performerIds")).toArray())
            group.performerIds.push_back(member.toString());
        return group;
    }
};

struct Performer {
    QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QString label;
    QString name;
    QString section;
    QString instrument;
    QString symbol;
    QColor color{QStringLiteral("#38bdf8")};
    QString notes;
    PerformerAppearance appearance;
    bool selected = false;
    bool visible = true;
    bool locked = false;

    QJsonObject toJson() const
    {
        return {{QStringLiteral("id"), id},
                {QStringLiteral("label"), label},
                {QStringLiteral("name"), name},
                {QStringLiteral("section"), section},
                {QStringLiteral("instrument"), instrument},
                {QStringLiteral("symbol"), symbol},
                {QStringLiteral("color"), color.name(QColor::HexRgb)},
                {QStringLiteral("notes"), notes},
                {QStringLiteral("appearance"), appearance.toJson()},
                {QStringLiteral("visible"), visible},
                {QStringLiteral("locked"), locked}};
    }

    static Performer fromJson(const QJsonObject &object)
    {
        Performer result;
        result.id = object.value(QStringLiteral("id")).toString(result.id);
        result.label = object.value(QStringLiteral("label")).toString();
        result.name = object.value(QStringLiteral("name")).toString();
        result.section = object.value(QStringLiteral("section")).toString();
        result.instrument = object.value(QStringLiteral("instrument")).toString();
        result.symbol = object.value(QStringLiteral("symbol")).toString();
        result.color = QColor(object.value(QStringLiteral("color")).toString(QStringLiteral("#38bdf8")));
        result.notes = object.value(QStringLiteral("notes")).toString();
        result.appearance = PerformerAppearance::fromJson(object.value(QStringLiteral("appearance")).toObject());
        if (result.instrument.compare(QStringLiteral("Guard"), Qt::CaseInsensitive) == 0
            && !object.contains(QStringLiteral("appearance"))) {
            result.appearance.instrumentAssetId = QStringLiteral("equipment.guard.flag");
            result.appearance.roleId = QStringLiteral("guard");
        }
        result.visible = object.value(QStringLiteral("visible")).toBool(true);
        result.locked = object.value(QStringLiteral("locked")).toBool();
        return result;
    }
};

struct SetVariant {
    QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QString label{QStringLiteral("A")};
    QString name;
    QString caption;
    QHash<QString, Placement> placements;
    QVector<FormationShape> shapes;
    QVector<PerformerGroup> groups;

    QJsonObject toJson() const
    {
        QJsonObject placementObject;
        for (auto it = placements.cbegin(); it != placements.cend(); ++it)
            placementObject.insert(it.key(), it.value().toJson());
        QJsonArray shapeArray;
        for (const auto &shape : shapes) shapeArray.push_back(shape.toJson());
        QJsonArray groupArray;
        for (const auto &group : groups) groupArray.push_back(group.toJson());
        return {{QStringLiteral("id"), id},
                {QStringLiteral("label"), label},
                {QStringLiteral("name"), name},
                {QStringLiteral("caption"), caption},
                {QStringLiteral("placements"), placementObject}, {QStringLiteral("shapes"), shapeArray},
                {QStringLiteral("groups"), groupArray}};
    }

    static SetVariant fromJson(const QJsonObject &object)
    {
        SetVariant result;
        result.id = object.value(QStringLiteral("id")).toString(result.id);
        result.label = object.value(QStringLiteral("label")).toString(QStringLiteral("A"));
        result.name = object.value(QStringLiteral("name")).toString();
        result.caption = object.value(QStringLiteral("caption")).toString();
        const auto placements = object.value(QStringLiteral("placements")).toObject();
        for (auto it = placements.begin(); it != placements.end(); ++it)
            result.placements.insert(it.key(), Placement::fromJson(it.value().toObject()));
        for (const auto &value : object.value(QStringLiteral("shapes")).toArray())
            result.shapes.push_back(FormationShape::fromJson(value.toObject()));
        for (const auto &value : object.value(QStringLiteral("groups")).toArray()) {
            auto group = PerformerGroup::fromJson(value.toObject());
            if (group.performerIds.size() >= 2) result.groups.push_back(std::move(group));
        }
        return result;
    }
};

struct DrillSet {
    QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QString number;
    QString measure;
    int counts = 8;
    qint64 startTick = 0;
    double stepMultiplier = 1.0;
    bool subset = false;
    QVector<SetVariant> variants;
    QVector<SetVariant> archivedVariants;
    QString activeVariantId;
    int archivedOrder = -1;

    DrillSet()
    {
        variants.push_back(SetVariant{});
        activeVariantId = variants.first().id;
    }

    int activeVariantIndex() const
    {
        for (int i = 0; i < variants.size(); ++i)
            if (variants[i].id == activeVariantId) return i;
        return variants.isEmpty() ? -1 : 0;
    }

    SetVariant &activeVariant()
    {
        if (variants.isEmpty()) variants.push_back(SetVariant{});
        int index = activeVariantIndex();
        if (index < 0) index = 0;
        activeVariantId = variants[index].id;
        return variants[index];
    }

    const SetVariant &activeVariant() const
    {
        return variants[qMax(0, activeVariantIndex())];
    }

    QJsonObject toJson() const
    {
        QJsonArray variantArray;
        for (const auto &variant : variants) variantArray.push_back(variant.toJson());
        QJsonArray archivedVariantArray;
        for (const auto &variant : archivedVariants) archivedVariantArray.push_back(variant.toJson());
        return {{QStringLiteral("id"), id},
                {QStringLiteral("number"), number},
                {QStringLiteral("measure"), measure},
                {QStringLiteral("counts"), counts},
                {QStringLiteral("startTick"), startTick},
                {QStringLiteral("stepMultiplier"), stepMultiplier},
                {QStringLiteral("subset"), subset},
                {QStringLiteral("activeVariantId"), activeVariantId},
                {QStringLiteral("variants"), variantArray},
                {QStringLiteral("archivedVariants"), archivedVariantArray},
                {QStringLiteral("archivedOrder"), archivedOrder}};
    }

    static DrillSet fromJson(const QJsonObject &object)
    {
        DrillSet result;
        result.variants.clear();
        result.id = object.value(QStringLiteral("id")).toString(result.id);
        result.number = object.value(QStringLiteral("number")).toString();
        result.measure = object.value(QStringLiteral("measure")).toString();
        result.counts = qMax(0, object.value(QStringLiteral("counts")).toInt(8));
        result.startTick = object.value(QStringLiteral("startTick")).toVariant().toLongLong();
        result.stepMultiplier = qBound(0.0, object.value(QStringLiteral("stepMultiplier")).toDouble(1.0), 2.0);
        result.subset = object.value(QStringLiteral("subset")).toBool();
        result.archivedOrder = object.value(QStringLiteral("archivedOrder")).toInt(-1);
        const auto variants = object.value(QStringLiteral("variants")).toArray();
        for (const auto &value : variants)
            result.variants.push_back(SetVariant::fromJson(value.toObject()));
        const auto archivedVariants = object.value(QStringLiteral("archivedVariants")).toArray();
        for (const auto &value : archivedVariants)
            result.archivedVariants.push_back(SetVariant::fromJson(value.toObject()));
        if (result.variants.isEmpty()) {
            SetVariant legacy;
            legacy.name = object.value(QStringLiteral("name")).toString();
            legacy.caption = object.value(QStringLiteral("caption")).toString();
            const auto placements = object.value(QStringLiteral("placements")).toObject();
            for (auto it = placements.begin(); it != placements.end(); ++it)
                legacy.placements.insert(it.key(), Placement::fromJson(it.value().toObject()));
            result.variants.push_back(std::move(legacy));
        }
        result.activeVariantId = object.value(QStringLiteral("activeVariantId")).toString();
        if (result.activeVariantIndex() < 0)
            result.activeVariantId = result.variants.first().id;
        return result;
    }
};

struct FieldGeometry {
    QString id;
    QString label;
    double frontHash = 28.4444444444;
    double backHash = 56.8888888889;
    double depth = 85.3333333333;
};

inline FieldGeometry fieldGeometry(const QString &preset)
{
    // Distances use the standard 8-to-5 marching step (22.5 inches).
    // The playing surface is 160 feet / 85 1/3 steps from sideline to sideline.
    if (preset == QStringLiteral("college"))
        return {preset, QStringLiteral("College"), 32.0, 53.3333333333, 85.3333333333};
    if (preset == QStringLiteral("nfl"))
        return {preset, QStringLiteral("Professional"), 37.7333333333, 47.6, 85.3333333333};
    if (preset == QStringLiteral("indoor"))
        return {preset, QStringLiteral("Indoor"), 15.0, 35.0, 50.0};
    return {QStringLiteral("hs"), QStringLiteral("High School"),
            28.4444444444, 56.8888888889, 85.3333333333};
}

} // namespace MarchCraft
