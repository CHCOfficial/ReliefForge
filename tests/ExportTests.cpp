#include "TestHarness.hpp"
#include "core/HeightField.hpp"
#include "core/Mesh.hpp"
#include "export/StlExporter.hpp"
#include "export/VectorExporter.hpp"

#include <filesystem>
#include <fstream>
#include <iterator>

namespace {

rf::HeightField smallField() {
    rf::ReliefDimensions dimensions;
    dimensions.widthMm = 10.0;
    dimensions.heightMm = 10.0;
    rf::HeightField field(2, 2, dimensions);
    field.at(0, 0) = 2.0; field.at(1, 0) = 3.0;
    field.at(0, 1) = 3.0; field.at(1, 1) = 4.0;
    return field;
}

std::string readText(const std::filesystem::path& path) {
    std::ifstream stream(path);
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

} // namespace

RF_TEST("Binary STL has a valid header count and record length") {
    const auto mesh = rf::MeshGenerator::rectangularSolid(smallField());
    const auto path = std::filesystem::temp_directory_path() / "reliefforge-test.stl";
    const auto exported = rf::StlExporter::write(mesh, path);
    RF_REQUIRE(rf::succeeded(exported));
    RF_REQUIRE(std::filesystem::file_size(path) == 84 + 50 * mesh.triangles().size());
    std::filesystem::remove(path);
}

RF_TEST("SVG is XML with physical dimensions and path entities") {
    const auto path = std::filesystem::temp_directory_path() / "reliefforge-test.svg";
    const auto vectors = rf::ContourGenerator::outerBoundary(smallField());
    RF_REQUIRE(rf::succeeded(rf::VectorExporter::writeSvg(vectors, path)));
    const auto text = readText(path);
    RF_REQUIRE(text.starts_with("<?xml"));
    RF_REQUIRE(text.find("width=\"10.00000mm\"") != std::string::npos);
    RF_REQUIRE(text.find("<path") != std::string::npos);
    std::filesystem::remove(path);
}

RF_TEST("DXF contains native lightweight polyline geometry") {
    const auto path = std::filesystem::temp_directory_path() / "reliefforge-test.dxf";
    const auto vectors = rf::ContourGenerator::outerBoundary(smallField());
    RF_REQUIRE(rf::succeeded(rf::VectorExporter::writeDxf(vectors, path)));
    const auto text = readText(path);
    RF_REQUIRE(text.find("LWPOLYLINE") != std::string::npos);
    RF_REQUIRE(text.find("OUTLINE") != std::string::npos);
    RF_REQUIRE(text.ends_with("0\nEOF\n"));
    std::filesystem::remove(path);
}

