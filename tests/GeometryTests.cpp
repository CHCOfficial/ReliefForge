#include "TestHarness.hpp"
#include "core/HeightField.hpp"
#include "core/Mesh.hpp"
#include "core/ReliefPipeline.hpp"

namespace {

rf::HeightField flatField(std::size_t columns = 4, std::size_t rows = 3) {
    rf::ReliefDimensions dimensions;
    dimensions.widthMm = 40.0;
    dimensions.heightMm = 20.0;
    dimensions.baseThicknessMm = 2.0;
    dimensions.reliefDepthMm = 3.0;
    rf::HeightField field(columns, rows, dimensions);
    for (std::size_t y = 0; y < rows; ++y) {
        for (std::size_t x = 0; x < columns; ++x) {
            field.at(x, y) = 5.0;
        }
    }
    return field;
}

} // namespace

RF_TEST("Rectangular relief is a closed manifold solid") {
    const auto mesh = rf::MeshGenerator::rectangularSolid(flatField());
    const auto statistics = rf::MeshAnalyzer::analyze(mesh);
    RF_REQUIRE(statistics.closed());
    RF_REQUIRE(statistics.manifold());
    RF_REQUIRE(statistics.boundaryEdges == 0);
    RF_REQUIRE(statistics.nonManifoldEdges == 0);
    RF_REQUIRE(statistics.degenerateFaces == 0);
    RF_REQUIRE(statistics.signedVolumeMm3 > 0.0);
    RF_REQUIRE_NEAR(statistics.signedVolumeMm3, 40.0 * 20.0 * 5.0, 1.0e-7);
}

RF_TEST("Mesh dimensions match requested physical dimensions") {
    const auto mesh = rf::MeshGenerator::rectangularSolid(flatField());
    double minimumX = 1.0e9;
    double maximumX = -1.0e9;
    double minimumY = 1.0e9;
    double maximumY = -1.0e9;
    double minimumZ = 1.0e9;
    double maximumZ = -1.0e9;
    for (const auto& vertex : mesh.vertices()) {
        minimumX = std::min(minimumX, vertex.x); maximumX = std::max(maximumX, vertex.x);
        minimumY = std::min(minimumY, vertex.y); maximumY = std::max(maximumY, vertex.y);
        minimumZ = std::min(minimumZ, vertex.z); maximumZ = std::max(maximumZ, vertex.z);
    }
    RF_REQUIRE_NEAR(maximumX - minimumX, 40.0, 1.0e-9);
    RF_REQUIRE_NEAR(maximumY - minimumY, 20.0, 1.0e-9);
    RF_REQUIRE_NEAR(maximumZ - minimumZ, 5.0, 1.0e-9);
}

RF_TEST("Grid triangle count includes top bottom and every side") {
    constexpr std::size_t columns = 4;
    constexpr std::size_t rows = 3;
    const auto mesh = rf::MeshGenerator::rectangularSolid(flatField(columns, rows));
    const auto expected = 4 * (columns - 1) * (rows - 1) +
        4 * ((columns - 1) + (rows - 1));
    RF_REQUIRE(mesh.triangles().size() == expected);
}

RF_TEST("Smooth preview normals average the relief top without rounding the base") {
    auto field = flatField(3, 3);
    field.at(1, 1) = 8.0;
    const auto mesh = rf::MeshGenerator::rectangularSolid(field);
    const auto normals = mesh.smoothTopNormals();

    RF_REQUIRE(normals.size() == mesh.vertices().size());
    RF_REQUIRE_NEAR(normals[4].x * normals[4].x +
                    normals[4].y * normals[4].y +
                    normals[4].z * normals[4].z, 1.0, 1.0e-12);
    RF_REQUIRE(normals[4].z > 0.0);

    const auto topVertexCount = field.columns() * field.rows();
    for (std::size_t index = topVertexCount; index < normals.size(); ++index) {
        RF_REQUIRE_NEAR(normals[index].x, 0.0, 1.0e-12);
        RF_REQUIRE_NEAR(normals[index].y, 0.0, 1.0e-12);
        RF_REQUIRE_NEAR(normals[index].z, 0.0, 1.0e-12);
    }
}

RF_TEST("Smooth high-resolution field preserves samples bounds and dimensions") {
    rf::ReliefDimensions dimensions;
    dimensions.widthMm = 30.0;
    dimensions.heightMm = 20.0;
    dimensions.baseThicknessMm = 2.0;
    dimensions.reliefDepthMm = 4.0;
    rf::HeightField field(3, 3, dimensions);
    for (std::size_t y = 0; y < field.rows(); ++y) {
        for (std::size_t x = 0; x < field.columns(); ++x) {
            field.at(x, y) = 2.0 + x + y * 0.5;
        }
    }

    const auto smooth = field.resampledSmooth(7, 7);
    RF_REQUIRE(smooth.columns() == 7);
    RF_REQUIRE(smooth.rows() == 7);
    RF_REQUIRE_NEAR(smooth.dimensions().widthMm, dimensions.widthMm, 1.0e-12);
    RF_REQUIRE_NEAR(smooth.dimensions().heightMm, dimensions.heightMm, 1.0e-12);
    RF_REQUIRE_NEAR(smooth.at(0, 0), field.at(0, 0), 1.0e-12);
    RF_REQUIRE_NEAR(smooth.at(6, 6), field.at(2, 2), 1.0e-12);
    RF_REQUIRE_NEAR(smooth.at(3, 3), field.at(1, 1), 1.0e-12);
    RF_REQUIRE(smooth.minimumZ() >= field.minimumZ());
    RF_REQUIRE(smooth.maximumZ() <= field.maximumZ());
}

RF_TEST("Smooth high-resolution print mesh remains watertight and manifold") {
    auto field = flatField(4, 3);
    field.at(1, 1) = 7.0;
    field.at(2, 1) = 6.0;
    const auto smooth = field.resampledSmooth(10, 7);
    const auto mesh = rf::MeshGenerator::rectangularSolid(smooth);
    const auto statistics = rf::MeshAnalyzer::analyze(mesh);

    RF_REQUIRE(statistics.closed());
    RF_REQUIRE(statistics.manifold());
    RF_REQUIRE(statistics.vertices == 10 * 7 * 2);
    RF_REQUIRE(statistics.triangles > rf::MeshGenerator::rectangularSolid(field).triangles().size());
}

RF_TEST("Smooth print dimensions retain source samples without unbounded subdivision") {
    const auto medium = rf::ReliefPipeline::smoothPrintDimensions(flatField(256, 128));
    RF_REQUIRE(medium.first == 766);
    RF_REQUIRE(medium.second == 382);
    const auto high = rf::ReliefPipeline::smoothPrintDimensions(flatField(512, 512));
    RF_REQUIRE(high.first == 1023);
    RF_REQUIRE(high.second == 1023);
    const auto ultra = rf::ReliefPipeline::smoothPrintDimensions(flatField(1024, 2));
    RF_REQUIRE(ultra.first == 1024);
    const auto source = rf::ReliefPipeline::smoothPrintDimensions(flatField(1200, 2));
    RF_REQUIRE(source.first == 1200);
    bool rejected = false;
    try { (void)rf::ReliefPipeline::smoothPrintDimensions(rf::HeightField{}); }
    catch (const std::invalid_argument&) { rejected = true; }
    RF_REQUIRE(rejected);
}

RF_TEST("Smooth subdivision preserves every sample and bounds on sharp reliefs") {
    auto field = flatField(4, 3);
    for (std::size_t y = 0; y < field.rows(); ++y) {
        for (std::size_t x = 0; x < field.columns(); ++x) {
            field.at(x, y) = (x + y) % 2 ? 7.0 : 2.0;
        }
    }
    const auto smooth = field.resampledSmooth(10, 7);
    for (std::size_t y = 0; y < field.rows(); ++y) {
        for (std::size_t x = 0; x < field.columns(); ++x) {
            RF_REQUIRE_NEAR(smooth.at(x * 3, y * 3), field.at(x, y), 1.0e-12);
        }
    }
    RF_REQUIRE(smooth.minimumZ() >= 2.0);
    RF_REQUIRE(smooth.maximumZ() <= 7.0);
}

RF_TEST("Desktop pipeline retains original geometry alongside validated smooth print mesh") {
    rf::GrayImage image(3, 3);
    for (std::size_t y = 0; y < 3; ++y) {
        for (std::size_t x = 0; x < 3; ++x) image.at(x, y) = (x + y) * 0.2F;
    }
    const rf::PipelineParameters parameters;
    const auto original = rf::ReliefPipeline::build(image, parameters);
    const auto both = rf::ReliefPipeline::build(image, parameters, true);
    RF_REQUIRE(!original.smoothPrintMesh);
    RF_REQUIRE(both.smoothPrintMesh != nullptr);
    RF_REQUIRE(both.smoothPrintStatistics.manifold());
    RF_REQUIRE(both.smoothPrintStatistics.triangles > both.statistics.triangles);
    RF_REQUIRE(both.mesh.triangles().size() == original.mesh.triangles().size());
    for (std::size_t i = 0; i < original.mesh.vertices().size(); ++i) {
        RF_REQUIRE(both.mesh.vertices()[i].x == original.mesh.vertices()[i].x);
        RF_REQUIRE(both.mesh.vertices()[i].y == original.mesh.vertices()[i].y);
        RF_REQUIRE(both.mesh.vertices()[i].z == original.mesh.vertices()[i].z);
    }
}
