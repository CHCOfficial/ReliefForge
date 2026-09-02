#include "TestHarness.hpp"
#include "app/ReliefGeometry.hpp"
#include "export/StlExporter.hpp"

#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>

namespace {

rf::HeightField previewField() {
    rf::HeightField field(4, 3, {});
    for (std::size_t y = 0; y < field.rows(); ++y) {
        for (std::size_t x = 0; x < field.columns(); ++x) {
            field.at(x, y) = 2.0 + (x + y) * 0.2;
        }
    }
    field.at(1, 1) = 5.0;
    return field;
}

float floatAt(const char* bytes) {
    float value{};
    std::memcpy(&value, bytes, sizeof(value));
    return value;
}

} // namespace

RF_TEST("Preview toggle selects shared print or original mesh without changing either") {
    const auto field = previewField();
    const auto original = std::make_shared<const rf::Mesh>(rf::MeshGenerator::rectangularSolid(field));
    const auto smooth = std::make_shared<const rf::Mesh>(
        rf::MeshGenerator::rectangularSolid(field.resampledSmooth(10, 7)));
    rf::app::ReliefGeometry geometry;
    geometry.setMeshes(original, smooth);
    RF_REQUIRE(geometry.activeMesh() == smooth);
    const auto smoothBytes = geometry.vertexData();
    geometry.setSmoothShading(false);
    RF_REQUIRE(geometry.activeMesh() == original);
    RF_REQUIRE(geometry.vertexData().size() == original->triangles().size() * 18 * sizeof(float));
    geometry.setSmoothShading(true);
    RF_REQUIRE(geometry.activeMesh() == smooth);
    RF_REQUIRE(geometry.vertexData() == smoothBytes);
    geometry.setMeshes({}, {});
    RF_REQUIRE(!geometry.activeMesh());
    RF_REQUIRE(geometry.vertexData().isEmpty());
}

RF_TEST("Every rendered triangle position matches binary STL coordinates in both preview modes") {
    const auto field = previewField();
    const auto original = std::make_shared<const rf::Mesh>(rf::MeshGenerator::rectangularSolid(field));
    const auto smooth = std::make_shared<const rf::Mesh>(
        rf::MeshGenerator::rectangularSolid(field.resampledSmooth(10, 7)));
    rf::app::ReliefGeometry geometry;
    geometry.setMeshes(original, smooth);
    const auto path = std::filesystem::temp_directory_path() / "reliefforge-preview-match.stl";
    for (bool smoothMode : {false, true}) {
        geometry.setSmoothShading(smoothMode);
        const auto mesh = geometry.activeMesh();
        RF_REQUIRE(rf::succeeded(rf::StlExporter::write(*mesh, path)));
        const auto rendered = geometry.vertexData();
        std::ifstream stream(path, std::ios::binary);
        stream.seekg(80);
        std::uint32_t triangleCount{};
        stream.read(reinterpret_cast<char*>(&triangleCount), sizeof(triangleCount));
        RF_REQUIRE(triangleCount == mesh->triangles().size());
        RF_REQUIRE(std::filesystem::file_size(path) == 84 + 50 * triangleCount);
        for (std::size_t face = 0; face < triangleCount; ++face) {
            std::array<char, 50> record{};
            stream.read(record.data(), record.size());
            RF_REQUIRE(stream.good());
            for (std::size_t vertex = 0; vertex < 3; ++vertex) {
                for (std::size_t axis = 0; axis < 3; ++axis) {
                    const auto previewOffset = (face * 18 + vertex * 6 + axis) * sizeof(float);
                    const auto stlOffset = 12 + (vertex * 3 + axis) * sizeof(float);
                    RF_REQUIRE(floatAt(rendered.constData() + previewOffset) ==
                               floatAt(record.data() + stlOffset));
                }
            }
        }
    }
    std::filesystem::remove(path);
}
