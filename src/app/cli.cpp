#include "core/Image.hpp"
#include "core/ReliefPipeline.hpp"
#include "export/StlExporter.hpp"
#include "export/VectorExporter.hpp"
#ifdef RELIEFFORGE_HAS_OCCT
#include "cad/StepExporter.hpp"
#endif

#include <charconv>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string_view>

namespace {

void printUsage() {
    std::cout
        << "ReliefForge CLI 0.1\n\n"
        << "Usage: reliefforge-cli input.pgm [options]\n"
        << "  --width MM           Physical width (default 120)\n"
        << "  --height MM          Physical height (default preserves source aspect)\n"
        << "  --relief-depth MM    Relief depth (default 3)\n"
        << "  --base MM            Base thickness (default 2)\n"
        << "  --invert             Invert height mapping\n"
        << "  --samples N          Longest geometry edge sample count\n"
        << "  --export-stl PATH    Export a watertight binary STL\n"
        << "  --export-svg PATH    Export iso-height contour paths\n"
        << "  --export-dxf PATH    Export iso-height contour polylines\n"
        << "  --export-step PATH   Export a fitted B-rep STEP solid (requires OpenCascade)\n";
}

bool parseDouble(std::string_view text, double& value) {
    const auto [pointer, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    return error == std::errc{} && pointer == text.data() + text.size();
}

bool parseSize(std::string_view text, std::size_t& value) {
    const auto [pointer, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    return error == std::errc{} && pointer == text.data() + text.size();
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        printUsage();
        return 2;
    }
    const std::filesystem::path input = argv[1];
    rf::PipelineParameters parameters;
    parameters.resolution.preset = rf::ResolutionPreset::Custom;
    parameters.resolution.customColumns = 256;
    std::optional<std::filesystem::path> stlPath;
    std::optional<std::filesystem::path> svgPath;
    std::optional<std::filesystem::path> dxfPath;
    std::optional<std::filesystem::path> stepPath;
    std::optional<double> explicitHeight;

    for (int index = 2; index < argc; ++index) {
        const std::string_view option = argv[index];
        const auto argument = [&]() -> std::optional<std::string_view> {
            if (index + 1 >= argc) {
                return std::nullopt;
            }
            return argv[++index];
        };
        if (option == "--invert") {
            parameters.image.invert = true;
        } else if (option == "--width" || option == "--height" ||
                   option == "--relief-depth" || option == "--base") {
            const auto value = argument();
            double parsed{};
            if (!value || !parseDouble(*value, parsed) || parsed <= 0.0) {
                std::cerr << "Invalid positive value for " << option << "\n";
                return 2;
            }
            if (option == "--width") parameters.height.dimensions.widthMm = parsed;
            if (option == "--height") explicitHeight = parsed;
            if (option == "--relief-depth") parameters.height.dimensions.reliefDepthMm = parsed;
            if (option == "--base") parameters.height.dimensions.baseThicknessMm = parsed;
        } else if (option == "--samples") {
            const auto value = argument();
            std::size_t parsed{};
            if (!value || !parseSize(*value, parsed) || parsed < 2 || parsed > 8192) {
                std::cerr << "--samples must be between 2 and 8192.\n";
                return 2;
            }
            parameters.resolution.customColumns = parsed;
        } else if (option == "--export-stl" || option == "--export-svg" ||
                   option == "--export-dxf" || option == "--export-step") {
            const auto value = argument();
            if (!value) {
                std::cerr << "Missing path for " << option << "\n";
                return 2;
            }
            if (option == "--export-stl") stlPath = *value;
            if (option == "--export-svg") svgPath = *value;
            if (option == "--export-dxf") dxfPath = *value;
            if (option == "--export-step") stepPath = *value;
        } else if (option == "--help" || option == "-h") {
            printUsage();
            return 0;
        } else {
            std::cerr << "Unknown option: " << option << "\n";
            return 2;
        }
    }

    const auto loaded = rf::loadPortableGraymap(input);
    if (!rf::succeeded(loaded)) {
        std::cerr << std::get<rf::Error>(loaded).message << '\n';
        return 1;
    }
    const auto& image = std::get<rf::GrayImage>(loaded);
    parameters.height.dimensions.heightMm = explicitHeight.value_or(
        parameters.height.dimensions.widthMm * image.height() / static_cast<double>(image.width()));
    parameters.resolution.customRows = std::max<std::size_t>(
        2,
        std::lround(parameters.resolution.customColumns * image.height() /
                    static_cast<double>(image.width())));

    rf::PipelineResult result;
    try {
        result = rf::ReliefPipeline::build(image, parameters);
    } catch (const std::exception& error) {
        std::cerr << "Relief generation failed: " << error.what() << '\n';
        return 1;
    }

    const auto reportError = [](const auto& exportResult) {
        if (!rf::succeeded(exportResult)) {
            std::cerr << std::get<rf::Error>(exportResult).message << '\n';
            return true;
        }
        return false;
    };
    if (stlPath && reportError(rf::StlExporter::write(result.mesh, *stlPath))) return 1;
    const auto contours = rf::ContourGenerator::contours(result.heightField);
    if (svgPath && reportError(rf::VectorExporter::writeSvg(contours, *svgPath))) return 1;
    if (dxfPath && reportError(rf::VectorExporter::writeDxf(contours, *dxfPath))) return 1;
    if (stepPath) {
#ifdef RELIEFFORGE_HAS_OCCT
        if (reportError(rf::StepExporter::write(result.heightField, *stepPath))) return 1;
#else
        std::cerr << "STEP export is unavailable because this build was compiled without OpenCascade.\n";
        return 1;
#endif
    }
    if (!stlPath && !svgPath && !dxfPath && !stepPath) {
        std::cout << "No export was requested. Use --help for options.\n";
    }
    std::cout << result.heightField.dimensions().widthMm << " x "
              << result.heightField.dimensions().heightMm << " x "
              << result.heightField.maximumZ() << " mm | "
              << result.statistics.triangles << " triangles | "
              << (result.statistics.manifold() ? "Manifold" : "Needs repair") << '\n';
    return 0;
}
