#include "core/Mesh.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

namespace rf {
namespace {

struct Edge {
    std::uint32_t low{};
    std::uint32_t high{};

    bool operator==(const Edge&) const = default;
};

struct EdgeHash {
    std::size_t operator()(const Edge& edge) const noexcept {
        return (static_cast<std::size_t>(edge.low) << 32U) ^ edge.high;
    }
};

Edge orderedEdge(std::uint32_t a, std::uint32_t b) {
    return {std::min(a, b), std::max(a, b)};
}

struct VertexBits {
    std::uint64_t x{};
    std::uint64_t y{};
    std::uint64_t z{};

    bool operator==(const VertexBits&) const = default;
};

struct VertexBitsHash {
    std::size_t operator()(const VertexBits& bits) const noexcept {
        const auto h1 = std::hash<std::uint64_t>{}(bits.x);
        const auto h2 = std::hash<std::uint64_t>{}(bits.y);
        const auto h3 = std::hash<std::uint64_t>{}(bits.z);
        return h1 ^ (h2 << 1U) ^ (h3 << 2U);
    }
};

} // namespace

void Mesh::reserve(std::size_t vertexCount, std::size_t triangleCount) {
    vertices_.reserve(vertexCount);
    triangles_.reserve(triangleCount);
}

std::uint32_t Mesh::addVertex(Vec3 vertex) {
    if (vertices_.size() >= std::numeric_limits<std::uint32_t>::max()) {
        throw std::overflow_error("The mesh exceeds the 32-bit vertex index limit.");
    }
    vertices_.push_back(vertex);
    return static_cast<std::uint32_t>(vertices_.size() - 1);
}

void Mesh::addTriangle(std::uint32_t a, std::uint32_t b, std::uint32_t c) {
    if (a >= vertices_.size() || b >= vertices_.size() || c >= vertices_.size()) {
        throw std::out_of_range("A triangle references a missing vertex.");
    }
    triangles_.push_back({a, b, c});
}

Vec3 Mesh::faceNormal(const Triangle& triangle) const {
    const auto& a = vertices_.at(triangle[0]);
    const auto& b = vertices_.at(triangle[1]);
    const auto& c = vertices_.at(triangle[2]);
    return normalised(cross(b - a, c - a));
}

std::vector<Vec3> Mesh::smoothTopNormals() const {
    std::vector<Vec3> normals(vertices_.size());
    for (const auto& triangle : triangles_) {
        const auto& a = vertices_.at(triangle[0]);
        const auto& b = vertices_.at(triangle[1]);
        const auto& c = vertices_.at(triangle[2]);
        const auto areaVector = cross(b - a, c - a);

        // A relief made from a height field has consistently upward-facing top
        // triangles. Ignoring vertical and downward faces preserves crisp side
        // walls and base edges while smoothing only the visible relief surface.
        if (areaVector.z <= 1.0e-15) {
            continue;
        }
        for (const auto index : triangle) {
            normals[index].x += areaVector.x;
            normals[index].y += areaVector.y;
            normals[index].z += areaVector.z;
        }
    }
    for (auto& normal : normals) {
        normal = normalised(normal);
    }
    return normals;
}

Mesh MeshGenerator::rectangularSolid(const HeightField& field) {
    const auto columns = field.columns();
    const auto rows = field.rows();
    if (columns < 2 || rows < 2) {
        throw std::invalid_argument("A rectangular solid requires at least 2 x 2 samples.");
    }

    const auto planeVertexCount = columns * rows;
    const auto surfaceTriangleCount = 4 * (columns - 1) * (rows - 1);
    const auto sideTriangleCount = 4 * ((columns - 1) + (rows - 1));
    Mesh mesh;
    mesh.reserve(planeVertexCount * 2, surfaceTriangleCount + sideTriangleCount);

    const auto dx = field.dimensions().widthMm / static_cast<double>(columns - 1);
    const auto dy = field.dimensions().heightMm / static_cast<double>(rows - 1);
    const auto xOrigin = -field.dimensions().widthMm * 0.5;
    const auto yOrigin = -field.dimensions().heightMm * 0.5;
    for (std::size_t y = 0; y < rows; ++y) {
        for (std::size_t x = 0; x < columns; ++x) {
            mesh.addVertex({xOrigin + x * dx, yOrigin + y * dy, field.at(x, y)});
        }
    }
    for (std::size_t y = 0; y < rows; ++y) {
        for (std::size_t x = 0; x < columns; ++x) {
            mesh.addVertex({xOrigin + x * dx, yOrigin + y * dy, 0.0});
        }
    }

    const auto top = [columns](std::size_t x, std::size_t y) {
        return static_cast<std::uint32_t>(y * columns + x);
    };
    const auto bottom = [columns, planeVertexCount](std::size_t x, std::size_t y) {
        return static_cast<std::uint32_t>(planeVertexCount + y * columns + x);
    };

    for (std::size_t y = 0; y + 1 < rows; ++y) {
        for (std::size_t x = 0; x + 1 < columns; ++x) {
            const auto t00 = top(x, y);
            const auto t10 = top(x + 1, y);
            const auto t01 = top(x, y + 1);
            const auto t11 = top(x + 1, y + 1);
            mesh.addTriangle(t00, t10, t11);
            mesh.addTriangle(t00, t11, t01);

            const auto b00 = bottom(x, y);
            const auto b10 = bottom(x + 1, y);
            const auto b01 = bottom(x, y + 1);
            const auto b11 = bottom(x + 1, y + 1);
            mesh.addTriangle(b00, b11, b10);
            mesh.addTriangle(b00, b01, b11);
        }
    }

    const auto addSide = [&mesh](std::uint32_t topA, std::uint32_t topB,
                                 std::uint32_t bottomA, std::uint32_t bottomB) {
        mesh.addTriangle(topA, bottomA, bottomB);
        mesh.addTriangle(topA, bottomB, topB);
    };

    // Walk the perimeter counter-clockwise as seen from above. This gives all side
    // triangles outward-facing normals while reusing the surface boundary vertices.
    for (std::size_t x = 0; x + 1 < columns; ++x) {
        addSide(top(x, 0), top(x + 1, 0), bottom(x, 0), bottom(x + 1, 0));
    }
    for (std::size_t y = 0; y + 1 < rows; ++y) {
        addSide(top(columns - 1, y), top(columns - 1, y + 1),
                bottom(columns - 1, y), bottom(columns - 1, y + 1));
    }
    for (std::size_t x = columns - 1; x > 0; --x) {
        addSide(top(x, rows - 1), top(x - 1, rows - 1),
                bottom(x, rows - 1), bottom(x - 1, rows - 1));
    }
    for (std::size_t y = rows - 1; y > 0; --y) {
        addSide(top(0, y), top(0, y - 1), bottom(0, y), bottom(0, y - 1));
    }
    return mesh;
}

MeshStatistics MeshAnalyzer::analyze(const Mesh& mesh, double epsilon) {
    MeshStatistics result;
    result.vertices = mesh.vertices().size();
    result.triangles = mesh.triangles().size();

    std::unordered_map<Edge, unsigned, EdgeHash> edgeUses;
    edgeUses.reserve(result.triangles * 3);
    for (const auto& triangle : mesh.triangles()) {
        if (triangle[0] >= result.vertices || triangle[1] >= result.vertices ||
            triangle[2] >= result.vertices) {
            ++result.degenerateFaces;
            continue;
        }
        ++edgeUses[orderedEdge(triangle[0], triangle[1])];
        ++edgeUses[orderedEdge(triangle[1], triangle[2])];
        ++edgeUses[orderedEdge(triangle[2], triangle[0])];

        const auto& a = mesh.vertices()[triangle[0]];
        const auto& b = mesh.vertices()[triangle[1]];
        const auto& c = mesh.vertices()[triangle[2]];
        const auto areaVector = cross(b - a, c - a);
        const auto areaSquared = areaVector.x * areaVector.x +
            areaVector.y * areaVector.y + areaVector.z * areaVector.z;
        if (areaSquared <= epsilon * epsilon) {
            ++result.degenerateFaces;
        }
        result.signedVolumeMm3 += (
            a.x * (b.y * c.z - b.z * c.y) -
            a.y * (b.x * c.z - b.z * c.x) +
            a.z * (b.x * c.y - b.y * c.x)) / 6.0;
    }
    for (const auto& [edge, uses] : edgeUses) {
        static_cast<void>(edge);
        if (uses == 1) {
            ++result.boundaryEdges;
        } else if (uses != 2) {
            ++result.nonManifoldEdges;
        }
    }

    std::unordered_set<VertexBits, VertexBitsHash> unique;
    unique.reserve(result.vertices);
    for (const auto& vertex : mesh.vertices()) {
        const VertexBits bits{
            std::bit_cast<std::uint64_t>(vertex.x),
            std::bit_cast<std::uint64_t>(vertex.y),
            std::bit_cast<std::uint64_t>(vertex.z),
        };
        if (!unique.insert(bits).second) {
            ++result.duplicateVertices;
        }
    }
    return result;
}

} // namespace rf
