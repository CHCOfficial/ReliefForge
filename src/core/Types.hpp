#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <variant>

namespace rf {

struct Error {
    std::string message;
};

template <typename T>
using Result = std::variant<T, Error>;

template <typename T>
[[nodiscard]] bool succeeded(const Result<T>& result) {
    return std::holds_alternative<T>(result);
}

struct Vec2 {
    double x{};
    double y{};
};

struct Vec3 {
    double x{};
    double y{};
    double z{};

    [[nodiscard]] Vec3 operator-(const Vec3& rhs) const {
        return {x - rhs.x, y - rhs.y, z - rhs.z};
    }
};

[[nodiscard]] inline Vec3 cross(const Vec3& lhs, const Vec3& rhs) {
    return {
        lhs.y * rhs.z - lhs.z * rhs.y,
        lhs.z * rhs.x - lhs.x * rhs.z,
        lhs.x * rhs.y - lhs.y * rhs.x,
    };
}

[[nodiscard]] inline Vec3 normalised(const Vec3& value) {
    const auto length = std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
    if (length <= 1.0e-15) {
        return {};
    }
    return {value.x / length, value.y / length, value.z / length};
}

} // namespace rf

