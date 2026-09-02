#include "core/Project.hpp"

#include <charconv>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>

namespace rf {
namespace {

template <typename T>
bool parseNumber(const std::string& text, T& value) {
    const auto* begin = text.data();
    const auto* end = begin + text.size();
    const auto [pointer, error] = std::from_chars(begin, end, value);
    return error == std::errc{} && pointer == end;
}

std::string escape(std::string value) {
    std::string result;
    result.reserve(value.size());
    for (const auto character : value) {
        switch (character) {
        case '\\': result += "\\\\"; break;
        case '\n': result += "\\n"; break;
        case '=': result += "\\="; break;
        default: result += character; break;
        }
    }
    return result;
}

std::string unescape(std::string_view value) {
    std::string result;
    result.reserve(value.size());
    bool escaped{};
    for (const auto character : value) {
        if (escaped) {
            result += character == 'n' ? '\n' : character;
            escaped = false;
        } else if (character == '\\') {
            escaped = true;
        } else {
            result += character;
        }
    }
    if (escaped) {
        result += '\\';
    }
    return result;
}

std::size_t findSeparator(const std::string& line) {
    bool escaped{};
    for (std::size_t index = 0; index < line.size(); ++index) {
        if (escaped) {
            escaped = false;
        } else if (line[index] == '\\') {
            escaped = true;
        } else if (line[index] == '=') {
            return index;
        }
    }
    return std::string::npos;
}

std::string serialiseCurve(const HeightCurve& curve) {
    std::ostringstream stream;
    stream << std::setprecision(17);
    for (std::size_t index = 0; index < curve.controlPoints().size(); ++index) {
        if (index != 0) stream << ';';
        stream << curve.controlPoints()[index].x << ',' << curve.controlPoints()[index].y;
    }
    return stream.str();
}

Result<HeightCurve> parseCurve(const std::string& text, CurveInterpolation interpolation) {
    std::vector<Vec2> points;
    std::size_t start{};
    try {
        while (start <= text.size()) {
            const auto end = text.find(';', start);
            const auto token = text.substr(start, end == std::string::npos ? end : end - start);
            const auto comma = token.find(',');
            if (comma == std::string::npos) {
                return Error{"A height-curve control point is malformed."};
            }
            points.push_back({std::stod(token.substr(0, comma)), std::stod(token.substr(comma + 1))});
            if (end == std::string::npos) break;
            start = end + 1;
        }
        return HeightCurve(std::move(points), interpolation);
    } catch (const std::exception&) {
        return Error{"The saved height curve is invalid."};
    }
}

} // namespace

Result<std::monostate> ProjectSerializer::save(
    const ProjectDocument& project,
    const std::filesystem::path& path) {
    std::ofstream stream(path);
    if (!stream) {
        return Error{"Could not create project file: " + path.string()};
    }
    const auto& image = project.parameters.image;
    const auto& height = project.parameters.height;
    const auto& dimensions = height.dimensions;
    stream << std::setprecision(17)
           << "reliefforge.project=" << project.schemaVersion << '\n'
           << "application.version=" << escape(project.applicationVersion) << '\n'
           << "source.image=" << escape(project.sourceImage.generic_string()) << '\n'
           << "source.depthMap=" << escape(project.depthMap.generic_string()) << '\n'
           << "image.brightness=" << image.brightness << '\n'
           << "image.contrast=" << image.contrast << '\n'
           << "image.gamma=" << image.gamma << '\n'
           << "image.blackLevel=" << image.blackLevel << '\n'
           << "image.whiteLevel=" << image.whiteLevel << '\n'
           << "image.blurRadius=" << image.blurRadius << '\n'
           << "image.invert=" << (image.invert ? 1 : 0) << '\n'
           << "relief.style=" << static_cast<unsigned>(height.style) << '\n'
           << "relief.widthMm=" << dimensions.widthMm << '\n'
           << "relief.heightMm=" << dimensions.heightMm << '\n'
           << "relief.depthMm=" << dimensions.reliefDepthMm << '\n'
           << "relief.baseMm=" << dimensions.baseThicknessMm << '\n'
           << "relief.minimum=" << height.minimumHeight << '\n'
           << "relief.maximum=" << height.maximumHeight << '\n'
           << "relief.scale=" << height.heightScale << '\n'
           << "relief.offset=" << height.offset << '\n'
           << "relief.contourLevels=" << height.contourLevels << '\n'
           << "curve.interpolation=" << static_cast<unsigned>(height.curve.interpolation()) << '\n'
           << "curve.points=" << serialiseCurve(height.curve) << '\n'
           << "geometry.preset=" << static_cast<unsigned>(project.parameters.resolution.preset) << '\n'
           << "geometry.columns=" << project.parameters.resolution.customColumns << '\n'
           << "geometry.rows=" << project.parameters.resolution.customRows << '\n'
           << "camera.yaw=" << project.cameraYawDegrees << '\n'
           << "camera.pitch=" << project.cameraPitchDegrees << '\n'
           << "camera.distance=" << project.cameraDistance << '\n';
    if (!stream) {
        return Error{"Writing the project file failed."};
    }
    return std::monostate{};
}

Result<ProjectDocument> ProjectSerializer::load(const std::filesystem::path& path) {
    std::ifstream stream(path);
    if (!stream) {
        return Error{"Could not open project file: " + path.string()};
    }
    std::map<std::string, std::string> values;
    std::string line;
    while (std::getline(stream, line)) {
        if (line.empty() || line.starts_with('#')) {
            continue;
        }
        const auto separator = findSeparator(line);
        if (separator == std::string::npos) {
            return Error{"Malformed project entry: " + line};
        }
        values.emplace(line.substr(0, separator), unescape(std::string_view(line).substr(separator + 1)));
    }

    ProjectDocument project;
    const auto schema = values.find("reliefforge.project");
    if (schema == values.end() || !parseNumber(schema->second, project.schemaVersion)) {
        return Error{"This is not a valid ReliefForge project file."};
    }
    if (project.schemaVersion > CurrentProjectSchema) {
        return Error{"This project was created by a newer ReliefForge schema (" +
            std::to_string(project.schemaVersion) + ")."};
    }

    const auto text = [&values](const char* key, std::string& destination) {
        if (const auto iterator = values.find(key); iterator != values.end()) {
            destination = iterator->second;
        }
    };
    const auto number = [&values](const char* key, auto& destination) {
        if (const auto iterator = values.find(key); iterator != values.end()) {
            return parseNumber(iterator->second, destination);
        }
        return true;
    };

    text("application.version", project.applicationVersion);
    std::string source;
    std::string depthMap;
    text("source.image", source);
    text("source.depthMap", depthMap);
    project.sourceImage = source;
    project.depthMap = depthMap;

    auto& image = project.parameters.image;
    auto& height = project.parameters.height;
    auto& dimensions = height.dimensions;
    unsigned style = static_cast<unsigned>(height.style);
    unsigned preset = static_cast<unsigned>(project.parameters.resolution.preset);
    unsigned interpolation = static_cast<unsigned>(height.curve.interpolation());
    unsigned invert = image.invert ? 1U : 0U;
    const bool valid =
        number("image.brightness", image.brightness) &&
        number("image.contrast", image.contrast) &&
        number("image.gamma", image.gamma) &&
        number("image.blackLevel", image.blackLevel) &&
        number("image.whiteLevel", image.whiteLevel) &&
        number("image.blurRadius", image.blurRadius) &&
        number("image.invert", invert) &&
        number("relief.style", style) &&
        number("relief.widthMm", dimensions.widthMm) &&
        number("relief.heightMm", dimensions.heightMm) &&
        number("relief.depthMm", dimensions.reliefDepthMm) &&
        number("relief.baseMm", dimensions.baseThicknessMm) &&
        number("relief.minimum", height.minimumHeight) &&
        number("relief.maximum", height.maximumHeight) &&
        number("relief.scale", height.heightScale) &&
        number("relief.offset", height.offset) &&
        number("relief.contourLevels", height.contourLevels) &&
        number("curve.interpolation", interpolation) &&
        number("geometry.preset", preset) &&
        number("geometry.columns", project.parameters.resolution.customColumns) &&
        number("geometry.rows", project.parameters.resolution.customRows) &&
        number("camera.yaw", project.cameraYawDegrees) &&
        number("camera.pitch", project.cameraPitchDegrees) &&
        number("camera.distance", project.cameraDistance);
    if (!valid) {
        return Error{"A numeric field in the project is invalid."};
    }
    if (style > static_cast<unsigned>(ReliefStyle::Edge) ||
        preset > static_cast<unsigned>(ResolutionPreset::Custom) ||
        interpolation > static_cast<unsigned>(CurveInterpolation::Smooth)) {
        return Error{"The project refers to an unsupported style, curve interpolation, or geometry preset."};
    }
    image.invert = invert != 0;
    height.style = static_cast<ReliefStyle>(style);
    project.parameters.resolution.preset = static_cast<ResolutionPreset>(preset);
    if (const auto iterator = values.find("curve.points"); iterator != values.end()) {
        auto curve = parseCurve(iterator->second, static_cast<CurveInterpolation>(interpolation));
        if (!succeeded(curve)) {
            return std::get<Error>(std::move(curve));
        }
        height.curve = std::get<HeightCurve>(std::move(curve));
    }
    return project;
}

} // namespace rf
