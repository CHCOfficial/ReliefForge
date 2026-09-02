#include "core/ReliefPipeline.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace rf {

std::pair<std::size_t, std::size_t> ReliefPipeline::smoothPrintDimensions(
    const HeightField& field) {
    if (field.columns() < 2 || field.rows() < 2) {
        throw std::invalid_argument("A smooth print requires at least 2 x 2 samples.");
    }
    constexpr std::size_t minimumIntervals = 511;
    constexpr std::size_t maximumIntervals = 1023;
    const auto longestIntervals = std::max(field.columns() - 1, field.rows() - 1);
    const auto minimumFactor = std::max<std::size_t>(
        2, (minimumIntervals + longestIntervals - 1) / longestIntervals);
    const auto maximumFactor = std::max<std::size_t>(1, maximumIntervals / longestIntervals);
    const auto factor = std::min(minimumFactor, maximumFactor);
    // Integer subdivision retains every original sample. Already-dense source
    // fields are kept intact instead of silently decimating fabrication detail.
    return {(field.columns() - 1) * factor + 1, (field.rows() - 1) * factor + 1};
}

std::pair<std::size_t, std::size_t> ReliefPipeline::sampleDimensions(
    std::size_t sourceWidth,
    std::size_t sourceHeight,
    const GeometryResolution& resolution) {
    if (sourceWidth < 2 || sourceHeight < 2) {
        throw std::invalid_argument("A relief source must be at least 2 x 2 pixels.");
    }
    if (resolution.preset == ResolutionPreset::Source) {
        return {sourceWidth, sourceHeight};
    }
    if (resolution.preset == ResolutionPreset::Custom) {
        return {
            std::clamp(resolution.customColumns, std::size_t{2}, std::size_t{8192}),
            std::clamp(resolution.customRows, std::size_t{2}, std::size_t{8192}),
        };
    }

    std::size_t longestEdge{};
    switch (resolution.preset) {
    case ResolutionPreset::Draft: longestEdge = 96; break;
    case ResolutionPreset::Medium: longestEdge = 256; break;
    case ResolutionPreset::High: longestEdge = 512; break;
    case ResolutionPreset::Ultra: longestEdge = 1024; break;
    case ResolutionPreset::Source:
    case ResolutionPreset::Custom:
        break;
    }
    const auto scale = std::min(1.0, longestEdge / static_cast<double>(std::max(sourceWidth, sourceHeight)));
    return {
        std::max<std::size_t>(2, std::lround(sourceWidth * scale)),
        std::max<std::size_t>(2, std::lround(sourceHeight * scale)),
    };
}

PipelineResult ReliefPipeline::build(
    const GrayImage& source,
    const PipelineParameters& parameters,
    bool includeSmoothPrint) {
    if (source.empty()) {
        throw std::invalid_argument("Cannot build a relief without a source image.");
    }
    auto processed = ImageProcessor::process(source, parameters.image);
    const auto [columns, rows] = sampleDimensions(source.width(), source.height(), parameters.resolution);
    if (processed.width() != columns || processed.height() != rows) {
        processed = ImageProcessor::resized(processed, columns, rows);
    }
    auto heightField = HeightField::fromImage(processed, parameters.height);
    auto mesh = MeshGenerator::rectangularSolid(heightField);
    auto statistics = MeshAnalyzer::analyze(mesh);
    PipelineResult result{
        std::move(processed), std::move(heightField), std::move(mesh), statistics, {}, {}};
    if (includeSmoothPrint) {
        const auto [smoothColumns, smoothRows] = smoothPrintDimensions(result.heightField);
        const auto smoothField = result.heightField.resampledSmooth(smoothColumns, smoothRows);
        result.smoothPrintMesh = std::make_shared<const Mesh>(
            MeshGenerator::rectangularSolid(smoothField));
        result.smoothPrintStatistics = MeshAnalyzer::analyze(*result.smoothPrintMesh);
    }
    return result;
}

} // namespace rf
