#include "TestHarness.hpp"
#include "core/Project.hpp"

#include <filesystem>
#include <fstream>

RF_TEST("Project files round-trip editable parameters") {
    rf::ProjectDocument project;
    project.sourceImage = "assets/portrait image.png";
    project.parameters.image.contrast = 1.37;
    project.parameters.image.invert = true;
    project.parameters.height.dimensions.widthMm = 143.25;
    project.parameters.height.dimensions.reliefDepthMm = 4.75;
    project.parameters.height.style = rf::ReliefStyle::BasRelief;
    project.parameters.height.curve = rf::HeightCurve::strong();
    project.parameters.resolution.preset = rf::ResolutionPreset::High;
    const auto path = std::filesystem::temp_directory_path() / "roundtrip.reliefstudio";
    RF_REQUIRE(rf::succeeded(rf::ProjectSerializer::save(project, path)));
    const auto loaded = rf::ProjectSerializer::load(path);
    RF_REQUIRE(rf::succeeded(loaded));
    const auto& restored = std::get<rf::ProjectDocument>(loaded);
    RF_REQUIRE(restored.sourceImage == project.sourceImage);
    RF_REQUIRE_NEAR(restored.parameters.image.contrast, 1.37, 1.0e-12);
    RF_REQUIRE(restored.parameters.image.invert);
    RF_REQUIRE_NEAR(restored.parameters.height.dimensions.widthMm, 143.25, 1.0e-12);
    RF_REQUIRE(restored.parameters.height.style == rf::ReliefStyle::BasRelief);
    RF_REQUIRE(restored.parameters.height.curve.controlPoints().size() ==
               project.parameters.height.curve.controlPoints().size());
    RF_REQUIRE_NEAR(restored.parameters.height.curve.evaluate(0.25),
                    project.parameters.height.curve.evaluate(0.25), 1.0e-12);
    RF_REQUIRE(restored.parameters.resolution.preset == rf::ResolutionPreset::High);
    std::filesystem::remove(path);
}

RF_TEST("Newer project schemas fail with an understandable error") {
    const auto path = std::filesystem::temp_directory_path() / "future.reliefstudio";
    {
        std::ofstream stream(path);
        stream << "reliefforge.project=999\n";
    }
    const auto loaded = rf::ProjectSerializer::load(path);
    RF_REQUIRE(!rf::succeeded(loaded));
    RF_REQUIRE(std::get<rf::Error>(loaded).message.find("newer") != std::string::npos);
    std::filesystem::remove(path);
}
