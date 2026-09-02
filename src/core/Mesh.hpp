#pragma once

#include "core/HeightField.hpp"
#include "core/Types.hpp"

#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace rf {

using Triangle = std::array<std::uint32_t, 3>;

class Mesh {
public:
    [[nodiscard]] std::span<const Vec3> vertices() const noexcept { return vertices_; }
    [[nodiscard]] std::span<const Triangle> triangles() const noexcept { return triangles_; }
    [[nodiscard]] std::span<Vec3> vertices() noexcept { return vertices_; }
    [[nodiscard]] std::span<Triangle> triangles() noexcept { return triangles_; }

    void reserve(std::size_t vertexCount, std::size_t triangleCount);
    std::uint32_t addVertex(Vec3 vertex);
    void addTriangle(std::uint32_t a, std::uint32_t b, std::uint32_t c);
    [[nodiscard]] Vec3 faceNormal(const Triangle& triangle) const;
    [[nodiscard]] std::vector<Vec3> smoothTopNormals() const;

private:
    std::vector<Vec3> vertices_;
    std::vector<Triangle> triangles_;
};

struct MeshStatistics {
    std::size_t vertices{};
    std::size_t triangles{};
    std::size_t boundaryEdges{};
    std::size_t nonManifoldEdges{};
    std::size_t degenerateFaces{};
    std::size_t duplicateVertices{};
    double signedVolumeMm3{};

    [[nodiscard]] bool closed() const noexcept { return boundaryEdges == 0; }
    [[nodiscard]] bool manifold() const noexcept {
        return closed() && nonManifoldEdges == 0 && degenerateFaces == 0;
    }
};

class MeshGenerator {
public:
    [[nodiscard]] static Mesh rectangularSolid(const HeightField& field);
};

class MeshAnalyzer {
public:
    [[nodiscard]] static MeshStatistics analyze(const Mesh& mesh, double epsilon = 1.0e-9);
};

} // namespace rf
