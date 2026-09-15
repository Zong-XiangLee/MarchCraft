#include "AssetCatalog.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

#include <cmath>

namespace {
const QSet<QString> RequiredSockets{QStringLiteral("hand.left"), QStringLiteral("hand.right"),
                                    QStringLiteral("chest"), QStringLiteral("shoulder.left"),
                                    QStringLiteral("shoulder.right"), QStringLiteral("waist"),
                                    QStringLiteral("back"), QStringLiteral("head"),
                                    QStringLiteral("equipment")};

QVariantList &listForKind(const QString &kind, QVariantList &bodyRigs, QVariantList &uniforms,
                          QVariantList &instruments, QVariantList &props, QVariantList &venues)
{
    if (kind == QStringLiteral("bodyRig")) return bodyRigs;
    if (kind == QStringLiteral("uniform")) return uniforms;
    if (kind == QStringLiteral("instrument")) return instruments;
    if (kind == QStringLiteral("prop")) return props;
    return venues;
}
}

AssetCatalog::AssetCatalog(QObject *parent) : QObject(parent)
{
    load();
}

void AssetCatalog::load()
{
    QFile file(QStringLiteral(":/catalog/catalog.json"));
    if (!file.open(QIODevice::ReadOnly)) {
        m_errors.push_back(QStringLiteral("Built-in asset catalog is missing"));
        return;
    }
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        m_errors.push_back(QStringLiteral("Asset catalog JSON is invalid: %1").arg(parseError.errorString()));
        return;
    }
    for (const auto &value : document.object().value(QStringLiteral("assets")).toArray())
        validateAndInsert(value.toObject().toVariantMap());
}

void AssetCatalog::validateAndInsert(const QVariantMap &asset)
{
    const QString id = asset.value(QStringLiteral("id")).toString();
    const QString kind = asset.value(QStringLiteral("kind")).toString();
    if (id.isEmpty() || !id.contains(QLatin1Char('.'))) {
        m_errors.push_back(QStringLiteral("Asset has an invalid semantic id: %1").arg(id));
        return;
    }
    if (m_assets.contains(id)) {
        m_errors.push_back(QStringLiteral("Duplicate asset id: %1").arg(id));
        return;
    }
    if (asset.value(QStringLiteral("version")).toInt() < 1) {
        m_errors.push_back(QStringLiteral("%1 has no valid version").arg(id));
        return;
    }
    const double scaleMeters = asset.value(QStringLiteral("scaleMeters")).toDouble();
    const double footContact = asset.value(QStringLiteral("footContactMeters")).toDouble();
    if (!std::isfinite(scaleMeters) || scaleMeters <= 0.0 || scaleMeters > 1000.0) {
        m_errors.push_back(QStringLiteral("%1 has an invalid scale").arg(id));
        return;
    }
    if (!std::isfinite(footContact) || std::abs(footContact) > 0.002) {
        m_errors.push_back(QStringLiteral("%1 is not grounded at Y=0").arg(id));
        return;
    }
    if (kind == QStringLiteral("bodyRig")) {
        const auto socketList = asset.value(QStringLiteral("sockets")).toStringList();
        const QSet<QString> sockets(socketList.cbegin(), socketList.cend());
        const auto missing = RequiredSockets - sockets;
        if (!missing.isEmpty()) {
            m_errors.push_back(QStringLiteral("%1 is missing canonical sockets: %2")
                                   .arg(id, QStringList(missing.cbegin(), missing.cend()).join(QStringLiteral(", "))));
            return;
        }
    }
    if (kind != QStringLiteral("bodyRig") && kind != QStringLiteral("uniform")
        && kind != QStringLiteral("instrument") && kind != QStringLiteral("prop")
        && kind != QStringLiteral("venue")) {
        m_errors.push_back(QStringLiteral("%1 has unsupported kind %2").arg(id, kind));
        return;
    }
    m_assets.insert(id, asset);
    listForKind(kind, m_bodyRigs, m_uniforms, m_instruments, m_props, m_venues).push_back(asset);
}

QVariantMap AssetCatalog::asset(const QString &id) const
{
    if (m_assets.contains(id)) return m_assets.value(id);
    const QString kind = id.section(QLatin1Char('.'), 0, 0);
    return m_assets.value(fallbackId(kind));
}

QString AssetCatalog::fallbackId(const QString &kind) const
{
    if (kind == QStringLiteral("performer") || kind == QStringLiteral("bodyRig"))
        return QStringLiteral("performer.body.standard");
    if (kind == QStringLiteral("uniform")) return QStringLiteral("uniform.marchcraft.default");
    if (kind == QStringLiteral("instrument") || kind == QStringLiteral("equipment")) return QStringLiteral("instrument.generic");
    if (kind == QStringLiteral("prop")) return QStringLiteral("prop.box");
    return QStringLiteral("venue.rehearsal");
}
