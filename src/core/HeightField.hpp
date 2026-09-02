#pragma once

#include "core/HeightCurve.hpp"
#include "core/Image.hpp"

#include <cstddef>
#include <span>
#include <vector>

namespace rf {

enum class ReliefStyle {
    Standard,
    Inverted,
    BasRelief,
    HighRelief,
    Lithophane,
    Emboss,
    Deboss,
    Engraving,
    Contour,
    Edge,
};

struct ReliefDimensions {
    double widthMm{120.0};
    double heightMm{82.35};
    double reliefDepthMm{3.0};
    double baseThicknessMm{2.0};
};

struct HeightFieldParameters {
    ReliefDimensions dimensions;
    ReliefStyle style{ReliefStyle::Standard};
    HeightCurve curve{HeightCurve::linear()};
    double minimumHeight{};
    double maximumHeight{1.0};
    double heightScale{1.0};
    double offset{};
    unsigned contourLevels{8};
};

class HeightField {
public:
    HeightField() = default;
    HeightField(std::size_t columns, std::size_t rows, ReliefDimensions dimensions);

    [[nodiscard]] std::size_t columns() const noexcept { return columns_; }
    [[nodiscard]] std::size_t rows() const noexcept { return rows_; }
    [[nodiscard]] const ReliefDimensions& dimensions() const noexcept { return dimensions_; }
    [[nodiscard]] double at(std::size_t x, std::size_t y) const;
    double& at(std::size_t x, std::size_t y);
    [[nodiscard]] std::span<const double> samples() const noexcept { return samples_; }
    [[nodiscard]] double minimumZ() const;
    [[nodiscard]] double maximumZ() const;
    [[nodiscard]] HeightField resampledSmooth(
        std::size_t targetColumns,
        std::size_t targetRows) const;

    static HeightField fromImage(const GrayImage& image, const HeightFieldParameters& parameters);

private:
    std::size_t columns_{};
    std::size_t rows_{};
    ReliefDimensions dimensions_;
    std::vector<double> samples_;
};

} // namespace rf
