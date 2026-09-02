#include "core/HeightCurve.hpp"

#include <algorithm>
#include <stdexcept>

namespace rf {

HeightCurve::HeightCurve() : HeightCurve(linear()) {}

HeightCurve::HeightCurve(std::vector<Vec2> controlPoints, CurveInterpolation interpolation)
    : controlPoints_(std::move(controlPoints)), interpolation_(interpolation) {
    if (controlPoints_.size() < 2) {
        throw std::invalid_argument("A height curve needs at least two control points.");
    }
    std::ranges::sort(controlPoints_, {}, &Vec2::x);
    if (controlPoints_.front().x > 0.0 || controlPoints_.back().x < 1.0) {
        throw std::invalid_argument("A height curve must cover the complete [0, 1] input range.");
    }
    for (std::size_t index = 1; index < controlPoints_.size(); ++index) {
        if (controlPoints_[index - 1].x == controlPoints_[index].x) {
            throw std::invalid_argument("Height curve control point inputs must be unique.");
        }
    }
}

double HeightCurve::evaluate(double input) const {
    input = std::clamp(input, 0.0, 1.0);
    const auto upper = std::ranges::upper_bound(controlPoints_, input, {}, &Vec2::x);
    if (upper == controlPoints_.begin()) {
        return std::clamp(upper->y, 0.0, 1.0);
    }
    if (upper == controlPoints_.end()) {
        return std::clamp(controlPoints_.back().y, 0.0, 1.0);
    }

    const auto& right = *upper;
    const auto& left = *(upper - 1);
    auto t = (input - left.x) / (right.x - left.x);
    if (interpolation_ == CurveInterpolation::Smooth) {
        t = t * t * (3.0 - 2.0 * t);
    }
    return std::clamp(left.y * (1.0 - t) + right.y * t, 0.0, 1.0);
}

HeightCurve HeightCurve::linear() {
    return HeightCurve({{0.0, 0.0}, {1.0, 1.0}}, CurveInterpolation::Linear);
}

HeightCurve HeightCurve::soft() {
    return HeightCurve({{0.0, 0.0}, {0.25, 0.18}, {0.75, 0.82}, {1.0, 1.0}});
}

HeightCurve HeightCurve::strong() {
    return HeightCurve({{0.0, 0.0}, {0.25, 0.08}, {0.75, 0.92}, {1.0, 1.0}});
}

HeightCurve HeightCurve::shadowsEnhanced() {
    return HeightCurve({{0.0, 0.0}, {0.18, 0.34}, {0.55, 0.68}, {1.0, 1.0}});
}

HeightCurve HeightCurve::highlightsEnhanced() {
    return HeightCurve({{0.0, 0.0}, {0.45, 0.32}, {0.82, 0.66}, {1.0, 1.0}});
}

HeightCurve HeightCurve::midtonesEnhanced() {
    return HeightCurve({{0.0, 0.0}, {0.3, 0.18}, {0.5, 0.5}, {0.7, 0.82}, {1.0, 1.0}});
}

HeightCurve HeightCurve::basRelief() {
    return HeightCurve({{0.0, 0.0}, {0.12, 0.22}, {0.5, 0.5}, {0.88, 0.78}, {1.0, 1.0}});
}

HeightCurve HeightCurve::highRelief() {
    return HeightCurve({{0.0, 0.0}, {0.35, 0.12}, {0.65, 0.88}, {1.0, 1.0}});
}

} // namespace rf

