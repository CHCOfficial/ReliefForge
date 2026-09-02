#include "export/VectorExporter.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <numbers>

namespace rf {
namespace {

struct Segment {
    Vec2 a;
    Vec2 b;
    double level{};
};

double distanceSquared(const Vec2& a, const Vec2& b) {
    const auto dx = a.x - b.x;
    const auto dy = a.y - b.y;
    return dx * dx + dy * dy;
}

Vec2 interpolate(Vec2 a, Vec2 b, double va, double vb, double level) {
    const auto denominator = vb - va;
    const auto t = std::abs(denominator) < 1.0e-15
        ? 0.5
        : std::clamp((level - va) / denominator, 0.0, 1.0);
    return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t};
}

double pointLineDistance(const Vec2& point, const Vec2& a, const Vec2& b) {
    const auto dx = b.x - a.x;
    const auto dy = b.y - a.y;
    const auto lengthSquared = dx * dx + dy * dy;
    if (lengthSquared <= 1.0e-18) {
        return std::sqrt(distanceSquared(point, a));
    }
    const auto t = std::clamp(((point.x - a.x) * dx + (point.y - a.y) * dy) / lengthSquared, 0.0, 1.0);
    const Vec2 projection{a.x + t * dx, a.y + t * dy};
    return std::sqrt(distanceSquared(point, projection));
}

void simplifySection(
    std::span<const Vec2> points,
    std::size_t first,
    std::size_t last,
    double tolerance,
    std::vector<bool>& keep) {
    if (last <= first + 1) {
        return;
    }
    double greatestDistance{};
    std::size_t greatestIndex{};
    for (std::size_t index = first + 1; index < last; ++index) {
        const auto distance = pointLineDistance(points[index], points[first], points[last]);
        if (distance > greatestDistance) {
            greatestDistance = distance;
            greatestIndex = index;
        }
    }
    if (greatestDistance > tolerance) {
        keep[greatestIndex] = true;
        simplifySection(points, first, greatestIndex, tolerance, keep);
        simplifySection(points, greatestIndex, last, tolerance, keep);
    }
}

std::vector<Vec2> simplify(std::span<const Vec2> points, double tolerance) {
    if (points.size() <= 2 || tolerance <= 0.0) {
        return {points.begin(), points.end()};
    }
    std::vector<bool> keep(points.size());
    keep.front() = true;
    keep.back() = true;
    simplifySection(points, 0, points.size() - 1, tolerance, keep);
    std::vector<Vec2> result;
    result.reserve(points.size());
    for (std::size_t index = 0; index < points.size(); ++index) {
        if (keep[index]) {
            result.push_back(points[index]);
        }
    }
    return result;
}

std::vector<Polyline> joinSegments(
    std::vector<Segment> segments,
    double tolerance,
    double simplifyTolerance,
    double minimumFeatureSize) {
    std::vector<Polyline> paths;
    const auto toleranceSquared = tolerance * tolerance;
    while (!segments.empty()) {
        const auto seed = segments.back();
        segments.pop_back();
        Polyline path{{seed.a, seed.b}, false, seed.level, "CONTOURS"};

        bool extended = true;
        while (extended && !segments.empty()) {
            extended = false;
            for (auto iterator = segments.begin(); iterator != segments.end(); ++iterator) {
                if (std::abs(iterator->level - path.levelMm) > 1.0e-9) {
                    continue;
                }
                if (distanceSquared(path.points.back(), iterator->a) <= toleranceSquared) {
                    path.points.push_back(iterator->b);
                } else if (distanceSquared(path.points.back(), iterator->b) <= toleranceSquared) {
                    path.points.push_back(iterator->a);
                } else if (distanceSquared(path.points.front(), iterator->b) <= toleranceSquared) {
                    path.points.insert(path.points.begin(), iterator->a);
                } else if (distanceSquared(path.points.front(), iterator->a) <= toleranceSquared) {
                    path.points.insert(path.points.begin(), iterator->b);
                } else {
                    continue;
                }
                segments.erase(iterator);
                extended = true;
                break;
            }
        }
        path.closed = path.points.size() > 2 &&
            distanceSquared(path.points.front(), path.points.back()) <= toleranceSquared;
        if (path.closed) {
            path.points.back() = path.points.front();
        }
        path.points = simplify(path.points, simplifyTolerance);
        double length{};
        for (std::size_t index = 1; index < path.points.size(); ++index) {
            length += std::sqrt(distanceSquared(path.points[index - 1], path.points[index]));
        }
        if (path.points.size() >= 2 && length >= minimumFeatureSize) {
            paths.push_back(std::move(path));
        }
    }
    return paths;
}

void addCellSegments(
    std::vector<Segment>& output,
    const HeightField& field,
    std::size_t x,
    std::size_t y,
    double level) {
    const auto dx = field.dimensions().widthMm / static_cast<double>(field.columns() - 1);
    const auto dy = field.dimensions().heightMm / static_cast<double>(field.rows() - 1);
    const Vec2 p0{x * dx, y * dy};
    const Vec2 p1{(x + 1) * dx, y * dy};
    const Vec2 p2{(x + 1) * dx, (y + 1) * dy};
    const Vec2 p3{x * dx, (y + 1) * dy};
    const double v0 = field.at(x, y);
    const double v1 = field.at(x + 1, y);
    const double v2 = field.at(x + 1, y + 1);
    const double v3 = field.at(x, y + 1);
    const unsigned code = (v0 >= level ? 1U : 0U) |
        (v1 >= level ? 2U : 0U) |
        (v2 >= level ? 4U : 0U) |
        (v3 >= level ? 8U : 0U);
    if (code == 0U || code == 15U) {
        return;
    }

    const auto bottom = interpolate(p0, p1, v0, v1, level);
    const auto right = interpolate(p1, p2, v1, v2, level);
    const auto top = interpolate(p3, p2, v3, v2, level);
    const auto left = interpolate(p0, p3, v0, v3, level);
    const auto add = [&output, level](Vec2 a, Vec2 b) { output.push_back({a, b, level}); };
    switch (code) {
    case 1: case 14: add(left, bottom); break;
    case 2: case 13: add(bottom, right); break;
    case 3: case 12: add(left, right); break;
    case 4: case 11: add(right, top); break;
    case 6: case 9: add(bottom, top); break;
    case 7: case 8: add(left, top); break;
    case 5: {
        const auto centre = (v0 + v1 + v2 + v3) * 0.25;
        if (centre >= level) {
            add(left, top);
            add(bottom, right);
        } else {
            add(left, bottom);
            add(top, right);
        }
        break;
    }
    case 10: {
        const auto centre = (v0 + v1 + v2 + v3) * 0.25;
        if (centre >= level) {
            add(left, bottom);
            add(top, right);
        } else {
            add(left, top);
            add(bottom, right);
        }
        break;
    }
    default: break;
    }
}

} // namespace

VectorGeometry ContourGenerator::contours(
    const HeightField& field,
    const ContourOptions& options) {
    VectorGeometry result{field.dimensions().widthMm, field.dimensions().heightMm, {}};
    const auto count = std::max(1U, options.count);
    auto minimum = options.minimumHeightMm;
    auto maximum = options.maximumHeightMm;
    if (maximum <= minimum) {
        minimum = field.minimumZ();
        maximum = field.maximumZ();
    }
    if (maximum <= minimum) {
        return result;
    }

    std::vector<Segment> segments;
    for (unsigned contour = 1; contour <= count; ++contour) {
        const auto level = minimum + (maximum - minimum) * contour / static_cast<double>(count + 1);
        for (std::size_t y = 0; y + 1 < field.rows(); ++y) {
            for (std::size_t x = 0; x + 1 < field.columns(); ++x) {
                addCellSegments(segments, field, x, y, level);
            }
        }
    }
    result.paths = joinSegments(
        std::move(segments),
        std::max(options.joinToleranceMm, 1.0e-9),
        std::max(options.simplifyToleranceMm, 0.0),
        std::max(options.minimumFeatureSizeMm, 0.0));
    return result;
}

VectorGeometry ContourGenerator::outerBoundary(const HeightField& field) {
    VectorGeometry result{field.dimensions().widthMm, field.dimensions().heightMm, {}};
    result.paths.push_back({
        {{0.0, 0.0}, {result.widthMm, 0.0}, {result.widthMm, result.heightMm},
         {0.0, result.heightMm}, {0.0, 0.0}},
        true,
        0.0,
        "OUTLINE",
    });
    return result;
}

Result<std::monostate> VectorExporter::writeSvg(
    const VectorGeometry& geometry,
    const std::filesystem::path& path) {
    if (geometry.widthMm <= 0.0 || geometry.heightMm <= 0.0) {
        return Error{"Cannot export SVG with invalid physical dimensions."};
    }
    std::ofstream stream(path);
    if (!stream) {
        return Error{"Could not create SVG file: " + path.string()};
    }
    stream << std::fixed << std::setprecision(5)
           << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
           << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << geometry.widthMm
           << "mm\" height=\"" << geometry.heightMm << "mm\" viewBox=\"0 0 "
           << geometry.widthMm << ' ' << geometry.heightMm << "\">\n"
           << "  <g fill=\"none\" stroke=\"#000\" stroke-width=\"0.1\">\n";
    for (const auto& polyline : geometry.paths) {
        if (polyline.points.empty()) {
            continue;
        }
        stream << "    <path data-layer=\"" << polyline.layer << "\" data-height-mm=\""
               << polyline.levelMm << "\" d=\"M " << polyline.points.front().x << ' '
               << polyline.points.front().y;
        for (std::size_t index = 1; index < polyline.points.size(); ++index) {
            stream << " L " << polyline.points[index].x << ' ' << polyline.points[index].y;
        }
        if (polyline.closed) {
            stream << " Z";
        }
        stream << "\"/>\n";
    }
    stream << "  </g>\n</svg>\n";
    if (!stream) {
        return Error{"Writing the SVG file failed."};
    }
    return std::monostate{};
}

Result<std::monostate> VectorExporter::writeDxf(
    const VectorGeometry& geometry,
    const std::filesystem::path& path) {
    std::ofstream stream(path);
    if (!stream) {
        return Error{"Could not create DXF file: " + path.string()};
    }
    stream << std::fixed << std::setprecision(6)
           << "0\nSECTION\n2\nHEADER\n9\n$INSUNITS\n70\n4\n0\nENDSEC\n"
           << "0\nSECTION\n2\nENTITIES\n";
    for (const auto& polyline : geometry.paths) {
        if (polyline.points.size() < 2) {
            continue;
        }
        const auto pointCount = polyline.closed &&
                distanceSquared(polyline.points.front(), polyline.points.back()) < 1.0e-18
            ? polyline.points.size() - 1
            : polyline.points.size();
        stream << "0\nLWPOLYLINE\n8\n" << polyline.layer
               << "\n90\n" << pointCount << "\n70\n" << (polyline.closed ? 1 : 0) << '\n';
        for (std::size_t index = 0; index < pointCount; ++index) {
            stream << "10\n" << polyline.points[index].x
                   << "\n20\n" << polyline.points[index].y << '\n';
        }
    }
    stream << "0\nENDSEC\n0\nEOF\n";
    if (!stream) {
        return Error{"Writing the DXF file failed."};
    }
    return std::monostate{};
}

} // namespace rf
