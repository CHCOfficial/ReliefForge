#include "export/StlExporter.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>

namespace rf {
namespace {

void writeFloat(std::ostream& stream, float value) {
    static_assert(sizeof(float) == 4);
    std::array<char, 4> bytes{};
    std::memcpy(bytes.data(), &value, sizeof(value));
    stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

Result<std::monostate> writeBinary(
    const Mesh& mesh,
    const std::filesystem::path& path,
    const StlExportOptions& options) {
    if (mesh.triangles().size() > std::numeric_limits<std::uint32_t>::max()) {
        return Error{"Binary STL supports at most 4,294,967,295 triangles."};
    }
    std::ofstream stream(path, std::ios::binary);
    if (!stream) {
        return Error{"Could not create STL file: " + path.string()};
    }

    std::array<char, 80> header{};
    const auto headerText = "ReliefForge | " + options.solidName;
    std::memcpy(header.data(), headerText.data(), std::min(header.size(), headerText.size()));
    stream.write(header.data(), static_cast<std::streamsize>(header.size()));
    const auto triangleCount = static_cast<std::uint32_t>(mesh.triangles().size());
    stream.write(reinterpret_cast<const char*>(&triangleCount), sizeof(triangleCount));

    for (const auto& triangle : mesh.triangles()) {
        const auto normal = mesh.faceNormal(triangle);
        writeFloat(stream, static_cast<float>(normal.x));
        writeFloat(stream, static_cast<float>(normal.y));
        writeFloat(stream, static_cast<float>(normal.z));
        for (const auto index : triangle) {
            const auto& vertex = mesh.vertices()[index];
            writeFloat(stream, static_cast<float>(vertex.x));
            writeFloat(stream, static_cast<float>(vertex.y));
            writeFloat(stream, static_cast<float>(vertex.z));
        }
        const std::uint16_t attributes{};
        stream.write(reinterpret_cast<const char*>(&attributes), sizeof(attributes));
    }
    if (!stream) {
        return Error{"Writing the binary STL file failed."};
    }
    return std::monostate{};
}

Result<std::monostate> writeAscii(
    const Mesh& mesh,
    const std::filesystem::path& path,
    const StlExportOptions& options) {
    std::ofstream stream(path);
    if (!stream) {
        return Error{"Could not create STL file: " + path.string()};
    }
    stream << std::setprecision(9) << "solid " << options.solidName << '\n';
    for (const auto& triangle : mesh.triangles()) {
        const auto normal = mesh.faceNormal(triangle);
        stream << "  facet normal " << normal.x << ' ' << normal.y << ' ' << normal.z << "\n"
               << "    outer loop\n";
        for (const auto index : triangle) {
            const auto& vertex = mesh.vertices()[index];
            stream << "      vertex " << vertex.x << ' ' << vertex.y << ' ' << vertex.z << '\n';
        }
        stream << "    endloop\n  endfacet\n";
    }
    stream << "endsolid " << options.solidName << '\n';
    if (!stream) {
        return Error{"Writing the ASCII STL file failed."};
    }
    return std::monostate{};
}

} // namespace

Result<std::monostate> StlExporter::write(
    const Mesh& mesh,
    const std::filesystem::path& path,
    const StlExportOptions& options) {
    if (mesh.vertices().empty() || mesh.triangles().empty()) {
        return Error{"Cannot export an empty mesh."};
    }
    if (options.requireManifold) {
        const auto status = MeshAnalyzer::analyze(mesh);
        if (!status.manifold()) {
            return Error{
                "STL export stopped because the mesh is not a closed manifold solid. "
                "Boundary edges: " + std::to_string(status.boundaryEdges) +
                ", non-manifold edges: " + std::to_string(status.nonManifoldEdges) +
                ", degenerate faces: " + std::to_string(status.degenerateFaces) + "."};
        }
    }
    return options.encoding == StlEncoding::Binary
        ? writeBinary(mesh, path, options)
        : writeAscii(mesh, path, options);
}

} // namespace rf

