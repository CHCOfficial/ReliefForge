#include "TestHarness.hpp"
#include "cad/StepExporter.hpp"
#include "core/HeightField.hpp"

#include <BRepCheck_Analyzer.hxx>
#include <STEPControl_Reader.hxx>
#include <TopoDS_Shape.hxx>

#include <filesystem>
#include <fstream>
#include <iterator>

RF_TEST("STEP exporter writes and OpenCascade reads a valid AP242 B-rep solid") {
    rf::ReliefDimensions dimensions;
    dimensions.widthMm = 24.0;
    dimensions.heightMm = 16.0;
    dimensions.baseThicknessMm = 2.0;
    dimensions.reliefDepthMm = 3.0;
    rf::HeightField field(8, 6, dimensions);
    for (std::size_t y = 0; y < field.rows(); ++y) {
        for (std::size_t x = 0; x < field.columns(); ++x) {
            const auto nx = x / static_cast<double>(field.columns() - 1);
            const auto ny = y / static_cast<double>(field.rows() - 1);
            field.at(x, y) = 2.0 + 2.5 * (nx * nx + ny) / 2.0;
        }
    }

    const auto path = std::filesystem::temp_directory_path() / "reliefforge-roundtrip.step";
    rf::StepExportOptions options;
    options.quality = rf::StepSurfaceQuality::Draft;
    options.schema = rf::StepSchema::AP242;
    options.modelName = "ReliefForge Test Relief";
    const auto exported = rf::StepExporter::write(field, path, options);
    RF_REQUIRE(rf::succeeded(exported));
    RF_REQUIRE(std::filesystem::file_size(path) > 1000);

    std::ifstream stream(path);
    const std::string text{
        std::istreambuf_iterator<char>(stream),
        std::istreambuf_iterator<char>()};
    RF_REQUIRE(text.find("ISO-10303-21") != std::string::npos);
    RF_REQUIRE(text.find("AP242_MANAGED_MODEL_BASED_3D_ENGINEERING") != std::string::npos);
    RF_REQUIRE(text.find("ADVANCED_BREP_SHAPE_REPRESENTATION") != std::string::npos);
    RF_REQUIRE(text.find("ReliefForge Test Relief") != std::string::npos);

    STEPControl_Reader reader;
    RF_REQUIRE(reader.ReadFile(path.string().c_str()) == IFSelect_RetDone);
    RF_REQUIRE(reader.TransferRoots() > 0);
    const TopoDS_Shape shape = reader.OneShape();
    RF_REQUIRE(!shape.IsNull());
    RF_REQUIRE(BRepCheck_Analyzer(shape, Standard_True).IsValid());
    std::filesystem::remove(path);
}
