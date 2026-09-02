#pragma once

#include "core/Types.hpp"

#include <span>
#include <vector>

namespace rf {

enum class CurveInterpolation {
    Linear,
    Smooth,
};

class HeightCurve {
public:
    HeightCurve();
    explicit HeightCurve(std::vector<Vec2> controlPoints, CurveInterpolation interpolation = CurveInterpolation::Smooth);

    [[nodiscard]] double evaluate(double input) const;
    [[nodiscard]] std::span<const Vec2> controlPoints() const noexcept { return controlPoints_; }
    [[nodiscard]] CurveInterpolation interpolation() const noexcept { return interpolation_; }

    static HeightCurve linear();
    static HeightCurve soft();
    static HeightCurve strong();
    static HeightCurve shadowsEnhanced();
    static HeightCurve highlightsEnhanced();
    static HeightCurve midtonesEnhanced();
    static HeightCurve basRelief();
    static HeightCurve highRelief();

private:
    std::vector<Vec2> controlPoints_;
    CurveInterpolation interpolation_{CurveInterpolation::Smooth};
};

} // namespace rf

