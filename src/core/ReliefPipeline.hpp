#pragma once

#include "core/HeightField.hpp"
#include "core/Mesh.hpp"

#include <cstddef>
#include <memory>

namespace rf {

enum class ResolutionPreset {
    Draft,
    Medium,
    High,
    Ultra,
    Source,
    Custom,
};

struct GeometryResolution {
    ResolutionPreset preset{ResolutionPreset::Medium};
    std::size_t customColumns{256};
    std::size_t customRows{256};
};

struct PipelineParameters {
    ImageProcessingParameters image;
    HeightFieldParameters height;
    GeometryResolution resolution;
};

struct PipelineResult {
    GrayImage processedImage;
    HeightField heightField;
    Mesh mesh;
    MeshStatistics statistics;
    std::shared_ptr<const Mesh> smoothPrintMesh;
    MeshStatistics smoothPrintStatistics;
};

class ReliefPipeline {
public:
    [[nodiscard]] static PipelineResult build(
        const GrayImage& source,
        const PipelineParameters& parameters,
        bool includeSmoothPrint = false);

    [[nodiscard]] static std::pair<std::size_t, std::size_t> smoothPrintDimensions(
        const HeightField& field);

    [[nodiscard]] static std::pair<std::size_t, std::size_t> sampleDimensions(
        std::size_t sourceWidth,
        std::size_t sourceHeight,
        const GeometryResolution& resolution);
};

} // namespace rf
