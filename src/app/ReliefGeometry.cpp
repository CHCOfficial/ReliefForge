#include "app/ReliefGeometry.hpp"

#include <QByteArray>
#include <QVector3D>

#include <algorithm>
#include <cstring>
#include <limits>

namespace rf::app {

ReliefGeometry::ReliefGeometry(QQuick3DObject* parent) : QQuick3DGeometry(parent) {
    setPrimitiveType(QQuick3DGeometry::PrimitiveType::Triangles);
}

void ReliefGeometry::setMeshes(
    std::shared_ptr<const Mesh> original,
    std::shared_ptr<const Mesh> smooth) {
    originalMesh_ = std::move(original);
    smoothMesh_ = std::move(smooth);
    rebuildVertexData();
}

std::shared_ptr<const Mesh> ReliefGeometry::activeMesh() const noexcept {
    return smoothShading_ && smoothMesh_ ? smoothMesh_ : originalMesh_;
}

void ReliefGeometry::setSmoothShading(bool enabled) {
    if (smoothShading_ == enabled) {
        return;
    }
    smoothShading_ = enabled;
    rebuildVertexData();
    emit smoothShadingChanged();
}

void ReliefGeometry::rebuildVertexData() {
    clear();
    const auto selected = activeMesh();
    if (!selected) {
        update();
        return;
    }
    const auto& mesh = *selected;
    constexpr auto componentsPerVertex = 6;
    QByteArray data;
    data.resize(static_cast<qsizetype>(
        mesh.triangles().size() * 3 * componentsPerVertex * sizeof(float)));
    auto* output = reinterpret_cast<float*>(data.data());
    const auto smoothNormals = smoothShading_ ? mesh.smoothTopNormals() : std::vector<Vec3>{};

    Vec3 minimum{
        std::numeric_limits<double>::max(),
        std::numeric_limits<double>::max(),
        std::numeric_limits<double>::max(),
    };
    Vec3 maximum{
        std::numeric_limits<double>::lowest(),
        std::numeric_limits<double>::lowest(),
        std::numeric_limits<double>::lowest(),
    };
    for (const auto& triangle : mesh.triangles()) {
        const auto faceNormal = mesh.faceNormal(triangle);
        const auto smoothTopFace = smoothShading_ && faceNormal.z > 0.0;
        for (const auto index : triangle) {
            const auto& vertex = mesh.vertices()[index];
            const auto& normal = smoothTopFace && smoothNormals[index].z > 0.0
                ? smoothNormals[index]
                : faceNormal;
            *output++ = static_cast<float>(vertex.x);
            *output++ = static_cast<float>(vertex.y);
            *output++ = static_cast<float>(vertex.z);
            *output++ = static_cast<float>(normal.x);
            *output++ = static_cast<float>(normal.y);
            *output++ = static_cast<float>(normal.z);
            minimum.x = std::min(minimum.x, vertex.x);
            minimum.y = std::min(minimum.y, vertex.y);
            minimum.z = std::min(minimum.z, vertex.z);
            maximum.x = std::max(maximum.x, vertex.x);
            maximum.y = std::max(maximum.y, vertex.y);
            maximum.z = std::max(maximum.z, vertex.z);
        }
    }

    setStride(componentsPerVertex * sizeof(float));
    addAttribute(QQuick3DGeometry::Attribute::PositionSemantic, 0,
                 QQuick3DGeometry::Attribute::F32Type);
    addAttribute(QQuick3DGeometry::Attribute::NormalSemantic, 3 * sizeof(float),
                 QQuick3DGeometry::Attribute::F32Type);
    setVertexData(data);
    if (!mesh.vertices().empty()) {
        setBounds(
            QVector3D(minimum.x, minimum.y, minimum.z),
            QVector3D(maximum.x, maximum.y, maximum.z));
    }
    update();
}

} // namespace rf::app
