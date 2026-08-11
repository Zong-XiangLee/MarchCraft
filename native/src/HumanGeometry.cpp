#include "HumanGeometry.h"

#include <QFile>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QVector3D>
#include <QtEndian>

#include <array>
#include <algorithm>
#include <cstring>
#include <mutex>

namespace {

constexpr quint32 GlbMagic = 0x46546c67;
constexpr quint32 JsonChunk = 0x4e4f534a;
constexpr quint32 BinaryChunk = 0x004e4942;

#pragma pack(push, 1)
struct HumanVertex {
    float position[3];
    float normal[3];
    float texCoord[2];
    quint16 joints[4];
    float weights[4];
};
#pragma pack(pop)

static_assert(sizeof(HumanVertex) == 56);

struct GeometryCache {
    QByteArray vertices;
    QByteArray indices;
    QVector3D boundsMin;
    QVector3D boundsMax;
    QString error;
};

quint32 readU32(const QByteArray &bytes, qsizetype offset)
{
    if (offset < 0 || offset + 4 > bytes.size())
        return 0;
    return qFromLittleEndian<quint32>(reinterpret_cast<const uchar *>(bytes.constData() + offset));
}

struct AccessorView {
    const char *data = nullptr;
    qsizetype stride = 0;
    qsizetype count = 0;
    int componentType = 0;
};

AccessorView accessorView(const QJsonObject &document, const QByteArray &binary, int accessorIndex,
                          QString *error)
{
    const auto accessors = document.value(QStringLiteral("accessors")).toArray();
    const auto bufferViews = document.value(QStringLiteral("bufferViews")).toArray();
    if (accessorIndex < 0 || accessorIndex >= accessors.size()) {
        *error = QStringLiteral("Invalid accessor index %1").arg(accessorIndex);
        return {};
    }
    const auto accessor = accessors.at(accessorIndex).toObject();
    const int bufferViewIndex = accessor.value(QStringLiteral("bufferView")).toInt(-1);
    if (bufferViewIndex < 0 || bufferViewIndex >= bufferViews.size()) {
        *error = QStringLiteral("Accessor %1 has no valid buffer view").arg(accessorIndex);
        return {};
    }
    const auto bufferView = bufferViews.at(bufferViewIndex).toObject();
    const qsizetype offset = bufferView.value(QStringLiteral("byteOffset")).toInteger()
                             + accessor.value(QStringLiteral("byteOffset")).toInteger();
    const int componentType = accessor.value(QStringLiteral("componentType")).toInt();
    const QString type = accessor.value(QStringLiteral("type")).toString();
    const int componentBytes = componentType == 5123 ? 2 : 4;
    const int components = type == QStringLiteral("VEC2") ? 2
                         : type == QStringLiteral("VEC3") ? 3
                         : type == QStringLiteral("VEC4") ? 4 : 1;
    const qsizetype packedStride = componentBytes * components;
    const qsizetype stride = bufferView.value(QStringLiteral("byteStride")).toInteger(packedStride);
    const qsizetype count = accessor.value(QStringLiteral("count")).toInteger();
    if (offset < 0 || count < 0 || (count && offset + (count - 1) * stride + packedStride > binary.size())) {
        *error = QStringLiteral("Accessor %1 exceeds the GLB binary chunk").arg(accessorIndex);
        return {};
    }
    return {binary.constData() + offset, stride, count, componentType};
}

GeometryCache loadGeometry(int detailLevel)
{
    GeometryCache result;
    const QString suffix = detailLevel == 1 ? QStringLiteral("_lod1")
                         : detailLevel == 2 ? QStringLiteral("_lod2") : QString();
    QFile file(QStringLiteral(":/performer/human_performer%1.glb").arg(suffix));
    if (!file.open(QIODevice::ReadOnly)) {
        result.error = QStringLiteral("Unable to open bundled human performer GLB");
        return result;
    }
    const QByteArray bytes = file.readAll();
    if (bytes.size() < 20 || readU32(bytes, 0) != GlbMagic || readU32(bytes, 4) != 2
        || readU32(bytes, 8) != quint32(bytes.size())) {
        result.error = QStringLiteral("Bundled human performer is not a valid glTF 2.0 binary");
        return result;
    }

    QByteArray jsonBytes;
    QByteArray binary;
    qsizetype offset = 12;
    while (offset + 8 <= bytes.size()) {
        const quint32 length = readU32(bytes, offset);
        const quint32 type = readU32(bytes, offset + 4);
        offset += 8;
        if (offset + length > bytes.size()) {
            result.error = QStringLiteral("Invalid human performer GLB chunk length");
            return result;
        }
        if (type == JsonChunk)
            jsonBytes = bytes.mid(offset, length).trimmed();
        else if (type == BinaryChunk)
            binary = bytes.mid(offset, length);
        offset += length;
    }

    QJsonParseError parseError;
    const auto json = QJsonDocument::fromJson(jsonBytes, &parseError);
    if (!json.isObject() || parseError.error != QJsonParseError::NoError) {
        result.error = QStringLiteral("Invalid human performer GLB JSON: %1").arg(parseError.errorString());
        return result;
    }
    const auto document = json.object();
    const auto meshes = document.value(QStringLiteral("meshes")).toArray();
    if (meshes.isEmpty()) {
        result.error = QStringLiteral("Human performer GLB has no mesh");
        return result;
    }
    const auto primitives = meshes.first().toObject().value(QStringLiteral("primitives")).toArray();
    if (primitives.isEmpty()) {
        result.error = QStringLiteral("Human performer GLB has no mesh primitive");
        return result;
    }
    const auto primitive = primitives.first().toObject();
    const auto attributes = primitive.value(QStringLiteral("attributes")).toObject();
    QString accessorError;
    const auto positions = accessorView(document, binary, attributes.value(QStringLiteral("POSITION")).toInt(-1), &accessorError);
    const auto normals = accessorView(document, binary, attributes.value(QStringLiteral("NORMAL")).toInt(-1), &accessorError);
    const auto texCoords = accessorView(document, binary, attributes.value(QStringLiteral("TEXCOORD_0")).toInt(-1), &accessorError);
    const auto joints = accessorView(document, binary, attributes.value(QStringLiteral("JOINTS_0")).toInt(-1), &accessorError);
    const auto weights = accessorView(document, binary, attributes.value(QStringLiteral("WEIGHTS_0")).toInt(-1), &accessorError);
    const auto indices = accessorView(document, binary, primitive.value(QStringLiteral("indices")).toInt(-1), &accessorError);
    if (!accessorError.isEmpty()) {
        result.error = accessorError;
        return result;
    }
    if (positions.count <= 0 || normals.count != positions.count || texCoords.count != positions.count
        || joints.count != positions.count || weights.count != positions.count
        || positions.componentType != 5126 || normals.componentType != 5126
        || texCoords.componentType != 5126 || joints.componentType != 5123
        || weights.componentType != 5126 || indices.componentType != 5125) {
        result.error = QStringLiteral("Human performer GLB vertex layout does not match the canonical contract");
        return result;
    }

    result.vertices.resize(positions.count * qsizetype(sizeof(HumanVertex)));
    auto *destination = reinterpret_cast<HumanVertex *>(result.vertices.data());
    for (qsizetype row = 0; row < positions.count; ++row) {
        std::memcpy(destination[row].position, positions.data + row * positions.stride, sizeof(destination[row].position));
        std::memcpy(destination[row].normal, normals.data + row * normals.stride, sizeof(destination[row].normal));
        std::memcpy(destination[row].texCoord, texCoords.data + row * texCoords.stride, sizeof(destination[row].texCoord));
        std::memcpy(destination[row].joints, joints.data + row * joints.stride, sizeof(destination[row].joints));
        std::memcpy(destination[row].weights, weights.data + row * weights.stride, sizeof(destination[row].weights));
    }
    result.indices = QByteArray(indices.data, indices.count * qsizetype(sizeof(quint32)));

    const auto accessorArray = document.value(QStringLiteral("accessors")).toArray();
    const auto positionAccessor = accessorArray.at(attributes.value(QStringLiteral("POSITION")).toInt()).toObject();
    const auto minimum = positionAccessor.value(QStringLiteral("min")).toArray();
    const auto maximum = positionAccessor.value(QStringLiteral("max")).toArray();
    if (minimum.size() != 3 || maximum.size() != 3) {
        result.error = QStringLiteral("Human performer GLB has no finite position bounds");
        return result;
    }
    result.boundsMin = {float(minimum.at(0).toDouble()), float(minimum.at(1).toDouble()), float(minimum.at(2).toDouble())};
    result.boundsMax = {float(maximum.at(0).toDouble()), float(maximum.at(1).toDouble()), float(maximum.at(2).toDouble())};
    return result;
}

const GeometryCache &geometryCache(int detailLevel)
{
    static const std::array<GeometryCache, 3> caches{loadGeometry(0), loadGeometry(1), loadGeometry(2)};
    return caches.at(std::clamp(detailLevel, 0, 2));
}

} // namespace

HumanGeometry::HumanGeometry(QQuick3DObject *parent)
    : QQuick3DGeometry(parent)
{
    applyDetailLevel();
}

void HumanGeometry::setDetailLevel(int value)
{
    value = std::clamp(value, 0, 2);
    if (value == m_detailLevel)
        return;
    m_detailLevel = value;
    clear();
    applyDetailLevel();
    emit detailLevelChanged();
}

void HumanGeometry::applyDetailLevel()
{
    m_valid = false;
    m_errorString.clear();
    const auto &cache = geometryCache(m_detailLevel);
    if (!cache.error.isEmpty()) {
        m_errorString = cache.error;
        qWarning().noquote() << m_errorString;
        return;
    }

    setPrimitiveType(QQuick3DGeometry::PrimitiveType::Triangles);
    setStride(sizeof(HumanVertex));
    setVertexData(cache.vertices);
    setIndexData(cache.indices);
    setBounds(cache.boundsMin, cache.boundsMax);
    addAttribute(QQuick3DGeometry::Attribute::PositionSemantic, offsetof(HumanVertex, position),
                 QQuick3DGeometry::Attribute::F32Type);
    addAttribute(QQuick3DGeometry::Attribute::NormalSemantic, offsetof(HumanVertex, normal),
                 QQuick3DGeometry::Attribute::F32Type);
    addAttribute(QQuick3DGeometry::Attribute::TexCoordSemantic, offsetof(HumanVertex, texCoord),
                 QQuick3DGeometry::Attribute::F32Type);
    addAttribute(QQuick3DGeometry::Attribute::JointSemantic, offsetof(HumanVertex, joints),
                 QQuick3DGeometry::Attribute::U16Type);
    addAttribute(QQuick3DGeometry::Attribute::WeightSemantic, offsetof(HumanVertex, weights),
                 QQuick3DGeometry::Attribute::F32Type);
    addAttribute(QQuick3DGeometry::Attribute::IndexSemantic, 0,
                 QQuick3DGeometry::Attribute::U32Type);
    m_valid = true;
}
