#pragma once

#include "core/Types.hpp"

#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

namespace rf {

struct RgbaPixel {
    std::uint8_t r{};
    std::uint8_t g{};
    std::uint8_t b{};
    std::uint8_t a{255};
};

class GrayImage {
public:
    GrayImage() = default;
    GrayImage(std::size_t width, std::size_t height, float value = 0.0F);

    [[nodiscard]] std::size_t width() const noexcept { return width_; }
    [[nodiscard]] std::size_t height() const noexcept { return height_; }
    [[nodiscard]] bool empty() const noexcept { return pixels_.empty(); }
    [[nodiscard]] float at(std::size_t x, std::size_t y) const;
    float& at(std::size_t x, std::size_t y);
    [[nodiscard]] std::span<const float> pixels() const noexcept { return pixels_; }
    [[nodiscard]] std::span<float> pixels() noexcept { return pixels_; }

    static GrayImage fromRgba(
        std::size_t width,
        std::size_t height,
        std::span<const RgbaPixel> pixels,
        bool multiplyByAlpha = false);

private:
    std::size_t width_{};
    std::size_t height_{};
    std::vector<float> pixels_;
};

enum class GrayscaleMode {
    Luminance,
    Average,
    Red,
    Green,
    Blue,
};

struct ImageProcessingParameters {
    double brightness{};
    double contrast{1.0};
    double gamma{1.0};
    double blackLevel{};
    double whiteLevel{1.0};
    double blurRadius{};
    bool invert{};
};

class ImageProcessor {
public:
    [[nodiscard]] static GrayImage process(
        const GrayImage& source,
        const ImageProcessingParameters& parameters);
    [[nodiscard]] static GrayImage gaussianBlur(const GrayImage& source, double radius);
    [[nodiscard]] static GrayImage resized(
        const GrayImage& source,
        std::size_t width,
        std::size_t height);
};

Result<GrayImage> loadPortableGraymap(const std::filesystem::path& path);
Result<std::monostate> savePortableGraymap(
    const GrayImage& image,
    const std::filesystem::path& path);

} // namespace rf

