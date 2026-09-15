#pragma once

#include <QObject>
#include <QVariantList>

class AssetCatalog final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList bodyRigs READ bodyRigs CONSTANT)
    Q_PROPERTY(QVariantList uniforms READ uniforms CONSTANT)
    Q_PROPERTY(QVariantList instruments READ instruments CONSTANT)
    Q_PROPERTY(QVariantList props READ props CONSTANT)
    Q_PROPERTY(QVariantList venues READ venues CONSTANT)
    Q_PROPERTY(QStringList validationErrors READ validationErrors CONSTANT)
    Q_PROPERTY(bool valid READ valid CONSTANT)

public:
    explicit AssetCatalog(QObject *parent = nullptr);

    QVariantList bodyRigs() const { return m_bodyRigs; }
    QVariantList uniforms() const { return m_uniforms; }
    QVariantList instruments() const { return m_instruments; }
    QVariantList props() const { return m_props; }
    QVariantList venues() const { return m_venues; }
    QStringList validationErrors() const { return m_errors; }
    bool valid() const { return m_errors.isEmpty(); }

    Q_INVOKABLE QVariantMap asset(const QString &id) const;
    Q_INVOKABLE QString fallbackId(const QString &kind) const;
    Q_INVOKABLE bool contains(const QString &id) const { return m_assets.contains(id); }

private:
    void load();
    void validateAndInsert(const QVariantMap &asset);

    QHash<QString, QVariantMap> m_assets;
    QVariantList m_bodyRigs;
    QVariantList m_uniforms;
    QVariantList m_instruments;
    QVariantList m_props;
    QVariantList m_venues;
    QStringList m_errors;
};
