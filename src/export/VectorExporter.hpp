#pragma once

#include "core/HeightField.hpp"
#include "core/Types.hpp"

#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace rf {

struct Polyline {
    std::vector<Vec2> points;
    bool closed{};
    double levelMm{};
    std::string layer{"CONTOURS"};
};

struct VectorGeometry {
    double widthMm{};
    double heightMm{};
    std::vector<Polyline> paths;
};

struct ContourOptions {
    unsigned count{6};
    double minimumHeightMm{};
    double maximumHeightMm{};
    double simplifyToleranceMm{0.02};
    double joinToleranceMm{0.05};
    double minimumFeatureSizeMm{0.1};
};

class ContourGenerator {
public:
    [[nodiscard]] static VectorGeometry contours(
        const HeightField& field,
        const ContourOptions& options = {});
    [[nodiscard]] static VectorGeometry outerBoundary(const HeightField& field);
};

class VectorExporter {
public:
    static Result<std::monostate> writeSvg(
        const VectorGeometry& geometry,
        const std::filesystem::path& path);
    static Result<std::monostate> writeDxf(
        const VectorGeometry& geometry,
        const std::filesystem::path& path);
};

} // namespace rf
