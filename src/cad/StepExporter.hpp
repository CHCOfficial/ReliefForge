#pragma once

#include "core/HeightField.hpp"
#include "core/Types.hpp"

#include <filesystem>
#include <string>

namespace rf {

enum class StepSurfaceQuality {
    Draft,
    Standard,
    Fine,
    VeryFine,
};

enum class StepSchema {
    AP203,
    AP214,
    AP242,
};

struct StepExportOptions {
    StepSurfaceQuality quality{StepSurfaceQuality::Standard};
    StepSchema schema{StepSchema::AP242};
    double surfaceToleranceMm{0.05};
    double sewingToleranceMm{0.01};
    std::string modelName{"ReliefForge Relief"};
};

struct StepExportReport {
    std::size_t fittedColumns{};
    std::size_t fittedRows{};
    double maximumSampleDeviationMm{};
};

class StepExporter {
public:
    static Result<StepExportReport> write(
        const HeightField& field,
        const std::filesystem::path& path,
        const StepExportOptions& options = {});
};

} // namespace rf

