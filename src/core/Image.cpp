#include "core/Image.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace rf {
namespace {

float clamp01(double value) {
    return static_cast<float>(std::clamp(value, 0.0, 1.0));
}

std::string nextPgmToken(std::istream& stream) {
    std::string token;
    while (stream >> token) {
        if (!token.starts_with('#')) {
            return token;
        }
        stream.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    }
    return {};
}

} // namespace

GrayImage::GrayImage(std::size_t width, std::size_t height, float value)
    : width_(width), height_(height), pixels_(width * height, value) {
    if (width == 0 || height == 0) {
        throw std::invalid_argument("An image must have non-zero dimensions.");
    }
}

float GrayImage::at(std::size_t x, std::size_t y) const {
    return pixels_.at(y * width_ + x);
}

float& GrayImage::at(std::size_t x, std::size_t y) {
    return pixels_.at(y * width_ + x);
}

GrayImage GrayImage::fromRgba(
    std::size_t width,
    std::size_t height,
    std::span<const RgbaPixel> pixels,
    bool multiplyByAlpha) {
    if (pixels.size() != width * height) {
        throw std::invalid_argument("RGBA buffer size does not match its dimensions.");
    }

    GrayImage result(width, height);
    for (std::size_t index = 0; index < pixels.size(); ++index) {
        const auto& pixel = pixels[index];
        auto value = (0.2126 * pixel.r + 0.7152 * pixel.g + 0.0722 * pixel.b) / 255.0;
        if (multiplyByAlpha) {
            value *= pixel.a / 255.0;
        }
        result.pixels_[index] = clamp01(value);
    }
    return result;
}

GrayImage ImageProcessor::process(
    const GrayImage& source,
    const ImageProcessingParameters& parameters) {
    if (source.empty()) {
        return {};
    }

    auto result = parameters.blurRadius > 0.01
        ? gaussianBlur(source, parameters.blurRadius)
        : source;
    const auto range = std::max(parameters.whiteLevel - parameters.blackLevel, 1.0e-9);
    const auto safeGamma = std::max(parameters.gamma, 1.0e-6);

    for (auto& sample : result.pixels()) {
        auto value = (sample - parameters.blackLevel) / range;
        value = (value - 0.5) * parameters.contrast + 0.5 + parameters.brightness;
        value = std::pow(std::clamp(value, 0.0, 1.0), 1.0 / safeGamma);
        if (parameters.invert) {
            value = 1.0 - value;
        }
        sample = clamp01(value);
    }
    return result;
}

GrayImage ImageProcessor::gaussianBlur(const GrayImage& source, double radius) {
    if (source.empty() || radius <= 0.01) {
        return source;
    }

    const auto sigma = std::max(radius / 2.0, 0.35);
    const auto halfWidth = std::max(1, static_cast<int>(std::ceil(radius * 2.0)));
    std::vector<double> kernel(static_cast<std::size_t>(halfWidth * 2 + 1));
    double sum{};
    for (int offset = -halfWidth; offset <= halfWidth; ++offset) {
        const auto weight = std::exp(-(offset * offset) / (2.0 * sigma * sigma));
        kernel[static_cast<std::size_t>(offset + halfWidth)] = weight;
        sum += weight;
    }
    for (auto& weight : kernel) {
        weight /= sum;
    }

    GrayImage horizontal(source.width(), source.height());
    GrayImage result(source.width(), source.height());
    for (std::size_t y = 0; y < source.height(); ++y) {
        for (std::size_t x = 0; x < source.width(); ++x) {
            double value{};
            for (int offset = -halfWidth; offset <= halfWidth; ++offset) {
                const auto sx = static_cast<std::size_t>(std::clamp(
                    static_cast<long long>(x) + offset,
                    0LL,
                    static_cast<long long>(source.width() - 1)));
                value += source.at(sx, y) * kernel[static_cast<std::size_t>(offset + halfWidth)];
            }
            horizontal.at(x, y) = static_cast<float>(value);
        }
    }
    for (std::size_t y = 0; y < source.height(); ++y) {
        for (std::size_t x = 0; x < source.width(); ++x) {
            double value{};
            for (int offset = -halfWidth; offset <= halfWidth; ++offset) {
                const auto sy = static_cast<std::size_t>(std::clamp(
                    static_cast<long long>(y) + offset,
                    0LL,
                    static_cast<long long>(source.height() - 1)));
                value += horizontal.at(x, sy) * kernel[static_cast<std::size_t>(offset + halfWidth)];
            }
            result.at(x, y) = static_cast<float>(value);
        }
    }
    return result;
}

GrayImage ImageProcessor::resized(
    const GrayImage& source,
    std::size_t width,
    std::size_t height) {
    if (source.empty() || width == 0 || height == 0) {
        return {};
    }
    if (source.width() == width && source.height() == height) {
        return source;
    }

    GrayImage result(width, height);
    const auto sx = width > 1 ? static_cast<double>(source.width() - 1) / (width - 1) : 0.0;
    const auto sy = height > 1 ? static_cast<double>(source.height() - 1) / (height - 1) : 0.0;
    for (std::size_t y = 0; y < height; ++y) {
        const auto sourceY = y * sy;
        const auto y0 = static_cast<std::size_t>(sourceY);
        const auto y1 = std::min(y0 + 1, source.height() - 1);
        const auto fy = sourceY - y0;
        for (std::size_t x = 0; x < width; ++x) {
            const auto sourceX = x * sx;
            const auto x0 = static_cast<std::size_t>(sourceX);
            const auto x1 = std::min(x0 + 1, source.width() - 1);
            const auto fx = sourceX - x0;
            const auto top = source.at(x0, y0) * (1.0 - fx) + source.at(x1, y0) * fx;
            const auto bottom = source.at(x0, y1) * (1.0 - fx) + source.at(x1, y1) * fx;
            result.at(x, y) = static_cast<float>(top * (1.0 - fy) + bottom * fy);
        }
    }
    return result;
}

Result<GrayImage> loadPortableGraymap(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        return Error{"Could not open image: " + path.string()};
    }

    const auto magic = nextPgmToken(stream);
    if (magic != "P2" && magic != "P5") {
        return Error{"The CLI fallback loader accepts PGM P2/P5 files. The desktop app uses Qt for PNG, JPEG, TIFF, BMP, and WebP."};
    }

    try {
        const auto width = static_cast<std::size_t>(std::stoull(nextPgmToken(stream)));
        const auto height = static_cast<std::size_t>(std::stoull(nextPgmToken(stream)));
        const auto maximum = std::stoi(nextPgmToken(stream));
        if (width == 0 || height == 0 || maximum <= 0 || maximum > 65535) {
            return Error{"Invalid PGM dimensions or sample range."};
        }
        GrayImage image(width, height);
        if (magic == "P2") {
            for (auto& sample : image.pixels()) {
                const auto token = nextPgmToken(stream);
                if (token.empty()) {
                    return Error{"PGM file ended before all samples were read."};
                }
                sample = clamp01(std::stod(token) / maximum);
            }
        } else {
            stream.get();
            if (maximum <= 255) {
                std::vector<unsigned char> bytes(width * height);
                stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
                if (stream.gcount() != static_cast<std::streamsize>(bytes.size())) {
                    return Error{"PGM file ended before all samples were read."};
                }
                for (std::size_t index = 0; index < bytes.size(); ++index) {
                    image.pixels()[index] = static_cast<float>(bytes[index]) / maximum;
                }
            } else {
                for (auto& sample : image.pixels()) {
                    unsigned char bytes[2]{};
                    stream.read(reinterpret_cast<char*>(bytes), 2);
                    if (!stream) {
                        return Error{"PGM file ended before all samples were read."};
                    }
                    const auto value = static_cast<unsigned>(bytes[0]) * 256U + bytes[1];
                    sample = static_cast<float>(value) / maximum;
                }
            }
        }
        return image;
    } catch (const std::exception&) {
        return Error{"Invalid numeric value in PGM header."};
    }
}

Result<std::monostate> savePortableGraymap(
    const GrayImage& image,
    const std::filesystem::path& path) {
    if (image.empty()) {
        return Error{"Cannot save an empty image."};
    }
    std::ofstream stream(path, std::ios::binary);
    if (!stream) {
        return Error{"Could not create image: " + path.string()};
    }
    stream << "P5\n" << image.width() << ' ' << image.height() << "\n255\n";
    for (const auto sample : image.pixels()) {
        const auto byte = static_cast<unsigned char>(std::lround(std::clamp(sample, 0.0F, 1.0F) * 255.0F));
        stream.write(reinterpret_cast<const char*>(&byte), 1);
    }
    if (!stream) {
        return Error{"Writing the PGM image failed."};
    }
    return std::monostate{};
}

} // namespace rf
