#pragma once

#include <QColor>
#include <QJsonArray>
#include <QJsonObject>
#include <QPointF>
#include <QString>
#include <QStringList>
#include <QUuid>
#include <QVector3D>

#include <cmath>

namespace MarchCraft {

// Authoritative scene contract. Drill coordinates are expressed in 8-to-5
// marching steps; the renderer's right-handed, Y-up world is expressed in meters.
class FieldTransform final
{
public:
    static constexpr double MetersPerStep = 0.5715; // 22.5 inches
    static constexpr double FieldWidthSteps = 160.0;

    static QVector3D drillToWorld(const QPointF &position, double fieldDepthSteps,
                                  double surfaceOffsetMeters = 0.0)
    {
        return {static_cast<float>((position.x() - FieldWidthSteps / 2.0) * MetersPerStep),
                static_cast<float>(surfaceOffsetMeters),
                static_cast<float>((fieldDepthSteps / 2.0 - position.y()) * MetersPerStep)};
    }

    static QPointF worldToDrill(const QVector3D &position, double fieldDepthSteps)
    {
        return {position.x() / MetersPerStep + FieldWidthSteps / 2.0,
                fieldDepthSteps / 2.0 - position.z() / MetersPerStep};
    }
};

struct PerformerAppearance {
    QString bodyRigId{QStringLiteral("performer.body.standard")};
    QString uniformId{QStringLiteral("uniform.marchcraft.default")};
    QString skinPaletteId{QStringLiteral("skin.medium")};
    QString instrumentAssetId{QStringLiteral("instrument.generic")};
    QString equipmentAssetId;
    QString roleId{QStringLiteral("musician")};
    double heightMeters = 1.75;

    QJsonObject toJson() const
    {
        return {{QStringLiteral("bodyRigId"), bodyRigId},
                {QStringLiteral("uniformId"), uniformId},
                {QStringLiteral("skinPaletteId"), skinPaletteId},
                {QStringLiteral("instrumentAssetId"), instrumentAssetId},
                {QStringLiteral("equipmentAssetId"), equipmentAssetId},
                {QStringLiteral("roleId"), roleId},
                {QStringLiteral("heightMeters"), heightMeters}};
    }

    static PerformerAppearance fromJson(const QJsonObject &object)
    {
        PerformerAppearance appearance;
        appearance.bodyRigId = object.value(QStringLiteral("bodyRigId")).toString(appearance.bodyRigId);
        appearance.uniformId = object.value(QStringLiteral("uniformId")).toString(appearance.uniformId);
        appearance.skinPaletteId = object.value(QStringLiteral("skinPaletteId")).toString(appearance.skinPaletteId);
        appearance.instrumentAssetId = object.value(QStringLiteral("instrumentAssetId")).toString(appearance.instrumentAssetId);
        appearance.equipmentAssetId = object.value(QStringLiteral("equipmentAssetId")).toString();
        appearance.roleId = object.value(QStringLiteral("roleId")).toString(appearance.roleId);
        appearance.heightMeters = qBound(1.1, object.value(QStringLiteral("heightMeters")).toDouble(1.75), 2.25);
        return appearance;
    }
};

struct VenueConfiguration {
    QString venueId{QStringLiteral("venue.rehearsal")};
    QString lightingId{QStringLiteral("lighting.daylight")};
    QString graphicsProfile{QStringLiteral("automatic")};
    QColor primaryColor{QStringLiteral("#123f2c")};
    QColor secondaryColor{QStringLiteral("#d7b95b")};
    QColor turfColor{QStringLiteral("#1d5a3d")};
    QString scoreboardText{QStringLiteral("MARCHCRAFT")};
    double crowdDensity = 0.2;
    double fogDensity = 0.0;
    bool debugOverlay = false;

    QJsonObject toJson() const
    {
        return {{QStringLiteral("venueId"), venueId},
                {QStringLiteral("lightingId"), lightingId},
                {QStringLiteral("graphicsProfile"), graphicsProfile},
                {QStringLiteral("primaryColor"), primaryColor.name(QColor::HexRgb)},
                {QStringLiteral("secondaryColor"), secondaryColor.name(QColor::HexRgb)},
                {QStringLiteral("turfColor"), turfColor.name(QColor::HexRgb)},
                {QStringLiteral("scoreboardText"), scoreboardText},
                {QStringLiteral("crowdDensity"), crowdDensity},
                {QStringLiteral("fogDensity"), fogDensity},
                {QStringLiteral("debugOverlay"), debugOverlay}};
    }

    static VenueConfiguration fromJson(const QJsonObject &object)
    {
        VenueConfiguration configuration;
        configuration.venueId = object.value(QStringLiteral("venueId")).toString(configuration.venueId);
        configuration.lightingId = object.value(QStringLiteral("lightingId")).toString(configuration.lightingId);
        configuration.graphicsProfile = object.value(QStringLiteral("graphicsProfile")).toString(configuration.graphicsProfile);
        configuration.primaryColor = QColor(object.value(QStringLiteral("primaryColor")).toString(configuration.primaryColor.name()));
        configuration.secondaryColor = QColor(object.value(QStringLiteral("secondaryColor")).toString(configuration.secondaryColor.name()));
        configuration.turfColor = QColor(object.value(QStringLiteral("turfColor")).toString(configuration.turfColor.name()));
        configuration.scoreboardText = object.value(QStringLiteral("scoreboardText")).toString(configuration.scoreboardText).left(40);
        configuration.crowdDensity = qBound(0.0, object.value(QStringLiteral("crowdDensity")).toDouble(0.2), 1.0);
        configuration.fogDensity = qBound(0.0, object.value(QStringLiteral("fogDensity")).toDouble(), 1.0);
        configuration.debugOverlay = object.value(QStringLiteral("debugOverlay")).toBool();
        return configuration;
    }
};

struct PropMotion {
    QString pathType{QStringLiteral("direct")};
    QVector<QPointF> controlPoints;
    int startSet = 0;
    int endSet = 0;

    QJsonObject toJson() const
    {
        QJsonArray points;
        for (const auto &point : controlPoints)
            points.push_back(QJsonObject{{QStringLiteral("x"), point.x()}, {QStringLiteral("y"), point.y()}});
        return {{QStringLiteral("pathType"), pathType}, {QStringLiteral("controlPoints"), points},
                {QStringLiteral("startSet"), startSet}, {QStringLiteral("endSet"), endSet}};
    }

    static PropMotion fromJson(const QJsonObject &object)
    {
        PropMotion motion;
        motion.pathType = object.value(QStringLiteral("pathType")).toString(motion.pathType);
        motion.startSet = qMax(0, object.value(QStringLiteral("startSet")).toInt());
        motion.endSet = qMax(motion.startSet, object.value(QStringLiteral("endSet")).toInt(motion.startSet));
        for (const auto &value : object.value(QStringLiteral("controlPoints")).toArray()) {
            const auto point = value.toObject();
            motion.controlPoints.push_back({point.value(QStringLiteral("x")).toDouble(),
                                            point.value(QStringLiteral("y")).toDouble()});
        }
        return motion;
    }
};

struct PropInstance {
    QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QString definitionId{QStringLiteral("prop.box")};
    QPointF position{80.0, 42.0};
    double rotation = 0.0;
    QVector3D scale{1.0f, 1.0f, 1.0f};
    QString appearanceVariant{QStringLiteral("default")};
    QStringList assignedMoverIds;
    PropMotion motion;

    QJsonObject toJson() const
    {
        QJsonArray movers;
        for (const auto &idValue : assignedMoverIds) movers.push_back(idValue);
        return {{QStringLiteral("id"), id}, {QStringLiteral("definitionId"), definitionId},
                {QStringLiteral("x"), position.x()}, {QStringLiteral("y"), position.y()},
                {QStringLiteral("rotation"), rotation}, {QStringLiteral("scaleX"), scale.x()},
                {QStringLiteral("scaleY"), scale.y()}, {QStringLiteral("scaleZ"), scale.z()},
                {QStringLiteral("appearanceVariant"), appearanceVariant},
                {QStringLiteral("assignedMoverIds"), movers}, {QStringLiteral("motion"), motion.toJson()}};
    }

    static PropInstance fromJson(const QJsonObject &object)
    {
        PropInstance prop;
        prop.id = object.value(QStringLiteral("id")).toString(prop.id);
        prop.definitionId = object.value(QStringLiteral("definitionId")).toString(prop.definitionId);
        prop.position = {object.value(QStringLiteral("x")).toDouble(80.0), object.value(QStringLiteral("y")).toDouble(42.0)};
        prop.rotation = std::fmod(object.value(QStringLiteral("rotation")).toDouble() + 360.0, 360.0);
        prop.scale = {static_cast<float>(qBound(0.1, object.value(QStringLiteral("scaleX")).toDouble(1.0), 10.0)),
                      static_cast<float>(qBound(0.1, object.value(QStringLiteral("scaleY")).toDouble(1.0), 10.0)),
                      static_cast<float>(qBound(0.1, object.value(QStringLiteral("scaleZ")).toDouble(1.0), 10.0))};
        prop.appearanceVariant = object.value(QStringLiteral("appearanceVariant")).toString(prop.appearanceVariant);
        for (const auto &value : object.value(QStringLiteral("assignedMoverIds")).toArray())
            prop.assignedMoverIds.push_back(value.toString());
        prop.motion = PropMotion::fromJson(object.value(QStringLiteral("motion")).toObject());
        return prop;
    }
};

struct AnimationState {
    QString locomotion{QStringLiteral("idle")};
    QString pose{QStringLiteral("horn_down")};
    bool closesAtDestination = false;
    double normalizedTime = 0.0;
    double speedMetersPerSecond = 0.0;
    double travelDirectionDegrees = 0.0;
    double travelStepsPerCount = 0.0;
};

} // namespace MarchCraft
