#include "core/HeightField.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace rf {
namespace {

double catmullRom(double p0, double p1, double p2, double p3, double t) {
    const auto t2 = t * t;
    const auto t3 = t2 * t;
    return 0.5 * (
        2.0 * p1 +
        (-p0 + p2) * t +
        (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3) * t2 +
        (-p0 + 3.0 * p1 - 3.0 * p2 + p3) * t3);
}

double styleValue(
    const GrayImage& image,
    std::size_t x,
    std::size_t y,
    ReliefStyle style,
    unsigned contourLevels) {
    const auto value = static_cast<double>(image.at(x, y));
    switch (style) {
    case ReliefStyle::Inverted:
    case ReliefStyle::Lithophane:
    case ReliefStyle::Deboss:
    case ReliefStyle::Engraving:
        return 1.0 - value;
    case ReliefStyle::BasRelief:
        return 0.5 + std::tanh((value - 0.5) * 1.5) * 0.5;
    case ReliefStyle::HighRelief:
        return 0.5 + std::tanh((value - 0.5) * 3.0) * 0.5;
    case ReliefStyle::Emboss:
        return value >= 0.5 ? value : 0.0;
    case ReliefStyle::Contour: {
        const auto levels = std::max(2U, contourLevels);
        return std::round(value * (levels - 1)) / (levels - 1);
    }
    case ReliefStyle::Edge: {
        const auto left = image.at(x > 0 ? x - 1 : x, y);
        const auto right = image.at(std::min(x + 1, image.width() - 1), y);
        const auto above = image.at(x, y > 0 ? y - 1 : y);
        const auto below = image.at(x, std::min(y + 1, image.height() - 1));
        return std::clamp(std::hypot(right - left, below - above) * 2.0, 0.0, 1.0);
    }
    case ReliefStyle::Standard:
        return value;
    }
    return value;
}

} // namespace

HeightField::HeightField(std::size_t columns, std::size_t rows, ReliefDimensions dimensions)
    : columns_(columns), rows_(rows), dimensions_(dimensions), samples_(columns * rows) {
    if (columns < 2 || rows < 2) {
        throw std::invalid_argument("A height field needs at least 2 x 2 samples.");
    }
    if (dimensions.widthMm <= 0.0 || dimensions.heightMm <= 0.0 ||
        dimensions.reliefDepthMm < 0.0 || dimensions.baseThicknessMm <= 0.0) {
        throw std::invalid_argument("Relief dimensions must be physically valid.");
    }
}

double HeightField::at(std::size_t x, std::size_t y) const {
    return samples_.at(y * columns_ + x);
}

double& HeightField::at(std::size_t x, std::size_t y) {
    return samples_.at(y * columns_ + x);
}

double HeightField::minimumZ() const {
    return samples_.empty() ? 0.0 : *std::ranges::min_element(samples_);
}

double HeightField::maximumZ() const {
    return samples_.empty() ? 0.0 : *std::ranges::max_element(samples_);
}

HeightField HeightField::resampledSmooth(
    std::size_t targetColumns,
    std::size_t targetRows) const {
    if (columns_ < 2 || rows_ < 2) {
        throw std::invalid_argument("Cannot resample an empty height field.");
    }
    if (targetColumns < 2 || targetRows < 2) {
        throw std::invalid_argument("A resampled height field needs at least 2 x 2 samples.");
    }
    if (targetColumns == columns_ && targetRows == rows_) {
        return *this;
    }

    HeightField result(targetColumns, targetRows, dimensions_);
    const auto sourceScaleX =
        static_cast<double>(columns_ - 1) / static_cast<double>(targetColumns - 1);
    const auto sourceScaleY =
        static_cast<double>(rows_ - 1) / static_cast<double>(targetRows - 1);

    const auto clampedIndex = [](long long value, std::size_t limit) {
        return static_cast<std::size_t>(std::clamp(
            value, 0LL, static_cast<long long>(limit - 1)));
    };

    for (std::size_t y = 0; y < targetRows; ++y) {
        const auto sourceY = y * sourceScaleY;
        const auto y1 = static_cast<long long>(std::floor(sourceY));
        const auto fy = sourceY - y1;
        const std::array<std::size_t, 4> sy{
            clampedIndex(y1 - 1, rows_),
            clampedIndex(y1, rows_),
            clampedIndex(y1 + 1, rows_),
            clampedIndex(y1 + 2, rows_),
        };

        for (std::size_t x = 0; x < targetColumns; ++x) {
            const auto sourceX = x * sourceScaleX;
            const auto x1 = static_cast<long long>(std::floor(sourceX));
            const auto fx = sourceX - x1;
            const std::array<std::size_t, 4> sx{
                clampedIndex(x1 - 1, columns_),
                clampedIndex(x1, columns_),
                clampedIndex(x1 + 1, columns_),
                clampedIndex(x1 + 2, columns_),
            };

            std::array<double, 4> rows{};
            auto localMinimum = std::numeric_limits<double>::max();
            auto localMaximum = std::numeric_limits<double>::lowest();
            for (std::size_t sampleY = 0; sampleY < 4; ++sampleY) {
                std::array<double, 4> values{};
                for (std::size_t sampleX = 0; sampleX < 4; ++sampleX) {
                    values[sampleX] = at(sx[sampleX], sy[sampleY]);
                    localMinimum = std::min(localMinimum, values[sampleX]);
                    localMaximum = std::max(localMaximum, values[sampleX]);
                }
                rows[sampleY] = catmullRom(
                    values[0], values[1], values[2], values[3], fx);
            }
            result.at(x, y) = std::clamp(
                catmullRom(rows[0], rows[1], rows[2], rows[3], fy),
                localMinimum,
                localMaximum);
        }
    }
    return result;
}

HeightField HeightField::fromImage(
    const GrayImage& image,
    const HeightFieldParameters& parameters) {
    if (image.width() < 2 || image.height() < 2) {
        throw std::invalid_argument("A relief source image needs at least 2 x 2 samples.");
    }
    HeightField result(image.width(), image.height(), parameters.dimensions);
    const auto minimum = std::clamp(parameters.minimumHeight, 0.0, 1.0);
    const auto maximum = std::clamp(parameters.maximumHeight, minimum, 1.0);

    for (std::size_t y = 0; y < image.height(); ++y) {
        for (std::size_t x = 0; x < image.width(); ++x) {
            auto value = styleValue(image, x, y, parameters.style, parameters.contourLevels);
            value = parameters.curve.evaluate(value);
            value = std::clamp(value * parameters.heightScale + parameters.offset, minimum, maximum);
            result.at(x, y) = parameters.dimensions.baseThicknessMm +
                value * parameters.dimensions.reliefDepthMm;
        }
    }
    return result;
}

} // namespace rf
