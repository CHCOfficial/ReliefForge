#include "TestHarness.hpp"
#include "core/HeightField.hpp"
#include "export/VectorExporter.hpp"

namespace {

rf::HeightField rampField() {
    rf::ReliefDimensions dimensions;
    dimensions.widthMm = 20.0;
    dimensions.heightMm = 10.0;
    rf::HeightField field(5, 3, dimensions);
    for (std::size_t y = 0; y < field.rows(); ++y) {
        for (std::size_t x = 0; x < field.columns(); ++x) {
            field.at(x, y) = static_cast<double>(x);
        }
    }
    return field;
}

} // namespace

RF_TEST("Outer boundary is a closed physical rectangle") {
    const auto vectors = rf::ContourGenerator::outerBoundary(rampField());
    RF_REQUIRE(vectors.paths.size() == 1);
    RF_REQUIRE(vectors.paths.front().closed);
    RF_REQUIRE(vectors.paths.front().layer == "OUTLINE");
    RF_REQUIRE(vectors.paths.front().points.size() == 5);
    RF_REQUIRE_NEAR(vectors.paths.front().points[2].x, 20.0, 1.0e-9);
    RF_REQUIRE_NEAR(vectors.paths.front().points[2].y, 10.0, 1.0e-9);
}

RF_TEST("Marching squares extracts contours from a ramp") {
    rf::ContourOptions options;
    options.count = 3;
    options.minimumHeightMm = 0.0;
    options.maximumHeightMm = 4.0;
    options.simplifyToleranceMm = 0.0;
    const auto vectors = rf::ContourGenerator::contours(rampField(), options);
    RF_REQUIRE(vectors.paths.size() == 3);
    for (const auto& path : vectors.paths) {
        RF_REQUIRE(path.points.size() >= 2);
        RF_REQUIRE(path.layer == "CONTOURS");
    }
}

