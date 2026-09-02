#pragma once

#include "core/Mesh.hpp"

#include <QQuick3DGeometry>
#include <memory>

namespace rf::app {

class ReliefGeometry final : public QQuick3DGeometry {
    Q_OBJECT
    Q_PROPERTY(bool smoothShading READ smoothShading WRITE setSmoothShading
               NOTIFY smoothShadingChanged)

public:
    explicit ReliefGeometry(QQuick3DObject* parent = nullptr);
    void setMeshes(std::shared_ptr<const Mesh> original, std::shared_ptr<const Mesh> smooth);
    [[nodiscard]] std::shared_ptr<const Mesh> activeMesh() const noexcept;
    [[nodiscard]] bool smoothShading() const noexcept { return smoothShading_; }
    void setSmoothShading(bool enabled);

signals:
    void smoothShadingChanged();

private:
    void rebuildVertexData();

    std::shared_ptr<const Mesh> originalMesh_;
    std::shared_ptr<const Mesh> smoothMesh_;
    bool smoothShading_{true};
};

} // namespace rf::app
