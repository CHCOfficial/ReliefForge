#include "TestHarness.hpp"
#include "app/AppController.hpp"

#include <QColor>
#include <QCryptographicHash>
#include <QFile>
#include <QImage>
#include <QTemporaryDir>

#include <array>
#include <limits>
#include <set>
#include <vector>

namespace {

void requireCurveSamples(const rf::app::AppController& controller, const rf::HeightCurve& curve) {
    const auto samples = controller.heightCurveSamples();
    RF_REQUIRE(samples.size() == 257);
    for (int i = 0; i < samples.size(); ++i) {
        const auto point = samples[i].toMap();
        const double x = static_cast<double>(i) / 256;
        RF_REQUIRE_NEAR(point.value("x").toDouble(), x, 1.0e-12);
        RF_REQUIRE_NEAR(point.value("y").toDouble(), curve.evaluate(x), 1.0e-12);
    }
}

} // namespace

RF_TEST("About documents include licence, credits and source details with a fixed allowlist") {
    rf::app::AppController controller;
    RF_REQUIRE(controller.legalDocument("COPYING").contains("GNU GENERAL PUBLIC LICENSE"));
    RF_REQUIRE(controller.legalDocument("CREDITS.md").contains("buymeacoffee.com/CHCOfficial"));
    RF_REQUIRE(controller.legalDocument("THIRD_PARTY_NOTICES.md").contains("Qt 6.11.1"));
    RF_REQUIRE(controller.legalDocument("CORRESPONDING_SOURCE.md").contains("1.0.0-dependency-sources.tar.gz"));
    RF_REQUIRE(controller.legalDocument("../../etc/passwd").isEmpty());
    RF_REQUIRE(controller.legalDocument("").isEmpty());
}

RF_TEST("Desktop defaults use zero blur, Ultra geometry and smooth print preview") {
    rf::app::AppController controller;
    RF_REQUIRE_NEAR(controller.blurRadius(), 0.0, 0.0);
    RF_REQUIRE(controller.resolutionPreset() == static_cast<int>(rf::ResolutionPreset::Ultra));
    RF_REQUIRE(controller.geometry()->smoothShading());
    RF_REQUIRE(controller.previewMaterial() == 0);
    RF_REQUIRE(controller.vertexCountText() == "0");
    RF_REQUIRE(controller.triangleCountText() == "0");
}

RF_TEST("Normal startup loads the bundled topographic waves once with print-ready defaults") {
    rf::app::AppController controller;
    int submissions = 0;
    QObject::connect(&controller, &rf::app::AppController::imageSubmitted, [&] { ++submissions; });
    controller.openStartup({"ReliefForge", "-psn_0_123"});
    RF_REQUIRE(controller.hasImage());
    RF_REQUIRE(controller.activeExampleId() == "topographic-waves");
    RF_REQUIRE(controller.sourceName() == "Topographic Waves");
    RF_REQUIRE(controller.sourceUrl() == QUrl("qrc:/examples/topographic-waves.png"));
    RF_REQUIRE(controller.sourceInfo().startsWith("256 × 256"));
    RF_REQUIRE(controller.errorText().isEmpty());
    RF_REQUIRE_NEAR(controller.blurRadius(), 0, 0);
    RF_REQUIRE(controller.resolutionPreset() == static_cast<int>(rf::ResolutionPreset::Ultra));
    RF_REQUIRE(controller.geometry()->smoothShading());
    RF_REQUIRE(submissions == 1);
}

RF_TEST("All twelve bundled examples decode uniquely and produce valid original and smooth solids") {
    rf::app::AppController controller;
    const auto catalog = controller.exampleCatalog();
    RF_REQUIRE(catalog.size() == 12);
    std::set<QString> ids;
    std::set<QByteArray> images;
    std::set<QString> levels;
    for (const auto& entry : catalog) {
        const auto item = entry.toMap();
        const auto id = item.value("id").toString();
        RF_REQUIRE(!id.isEmpty());
        RF_REQUIRE(!item.value("description").toString().isEmpty());
        RF_REQUIRE(ids.insert(id).second);
        levels.insert(item.value("level").toString());
        const QImage image(QStringLiteral(":") + item.value("imageUrl").toUrl().path());
        RF_REQUIRE(!image.isNull());
        RF_REQUIRE(image.width() == 256 && image.height() == 256);
        RF_REQUIRE(images.insert(QCryptographicHash::hash(
            QByteArrayView(reinterpret_cast<const char*>(image.constBits()), image.sizeInBytes()),
            QCryptographicHash::Sha256)).second);
        RF_REQUIRE(controller.loadExample(id));
        RF_REQUIRE(controller.activeExampleId() == id);
        RF_REQUIRE(controller.sourceName() == item.value("name").toString());
        RF_REQUIRE(controller.errorText().isEmpty());

        // Small samples keep this full original/smooth pipeline check quick;
        // the controller tests above verify the actual 256px bundled inputs.
        rf::GrayImage field(16, 16);
        for (int y = 0; y < 16; ++y) for (int x = 0; x < 16; ++x)
            field.at(x, y) = qGray(image.pixel(x * 17, y * 17)) / 255.0F;
        rf::PipelineParameters parameters;
        parameters.resolution.preset = rf::ResolutionPreset::Source;
        const auto result = rf::ReliefPipeline::build(field, parameters, true);
        RF_REQUIRE(result.statistics.manifold());
        RF_REQUIRE(result.smoothPrintStatistics.manifold());
    }
    RF_REQUIRE(levels.size() == 3);
}

RF_TEST("Example selection resets edits while unknown examples leave the current image intact") {
    rf::app::AppController controller;
    RF_REQUIRE(controller.loadExample("topographic-waves"));
    controller.setBlurRadius(2);
    controller.setGamma(2);
    controller.setWidthMm(80);
    controller.setReliefDepthMm(9);
    controller.setInverted(true);
    controller.setReliefStyle(8);
    controller.setCurvePreset(4);
    controller.setResolutionPreset(0);
    controller.setPreviewMaterial(3);
    controller.geometry()->setSmoothShading(false);
    RF_REQUIRE(controller.loadExample("spiral-bloom"));
    RF_REQUIRE_NEAR(controller.blurRadius(), 0, 0);
    RF_REQUIRE_NEAR(controller.gamma(), 1, 0);
    RF_REQUIRE_NEAR(controller.widthMm(), 120, 0);
    RF_REQUIRE_NEAR(controller.reliefDepthMm(), 3, 0);
    RF_REQUIRE(!controller.inverted());
    RF_REQUIRE(controller.curvePreset() == 0 && controller.reliefStyle() == 0);
    RF_REQUIRE(controller.previewMaterial() == 0);
    RF_REQUIRE(controller.resolutionPreset() == 3);
    RF_REQUIRE(controller.geometry()->smoothShading());
    const auto source = controller.sourceUrl();
    RF_REQUIRE(!controller.loadExample("../../COPYING"));
    RF_REQUIRE(controller.sourceUrl() == source);
    RF_REQUIRE(controller.activeExampleId() == "spiral-bloom");
    RF_REQUIRE(!controller.errorText().isEmpty());
}

RF_TEST("Example projects preserve settings and reopen after being moved without external images") {
    QTemporaryDir temporary;
    rf::app::AppController controller;
    RF_REQUIRE(controller.loadExample("organic-cells"));
    controller.setWidthMm(85);
    controller.setBlurRadius(1.5);
    controller.setCurvePreset(2);
    controller.setResolutionPreset(0);
    const auto original = temporary.filePath("original.reliefstudio");
    controller.saveProject(QUrl::fromLocalFile(original));
    RF_REQUIRE(controller.errorText().isEmpty());
    const auto saved = rf::ProjectSerializer::load(original.toStdString());
    RF_REQUIRE(rf::succeeded(saved));
    RF_REQUIRE(std::get<rf::ProjectDocument>(saved).sourceImage.generic_string() == "builtin:organic-cells");
    QTemporaryDir moved;
    const auto relocated = moved.filePath("moved.reliefstudio");
    RF_REQUIRE(QFile::copy(original, relocated));
    rf::app::AppController reopened;
    reopened.openStartup({"ReliefForge", relocated});
    RF_REQUIRE(reopened.errorText().isEmpty());
    RF_REQUIRE(reopened.activeExampleId() == "organic-cells");
    RF_REQUIRE_NEAR(reopened.widthMm(), 85, 0);
    RF_REQUIRE_NEAR(reopened.blurRadius(), 1.5, 0);
    RF_REQUIRE(reopened.resolutionPreset() == 0);
    requireCurveSamples(reopened, rf::HeightCurve::strong());
}

RF_TEST("Explicit startup images take priority and a failed image does not become the demo") {
    QTemporaryDir temporary;
    QImage image(7, 5, QImage::Format_RGB32);
    image.fill(Qt::white);
    const auto filename = temporary.filePath("own-image.png");
    RF_REQUIRE(image.save(filename));
    rf::app::AppController controller;
    int submissions = 0;
    QObject::connect(&controller, &rf::app::AppController::imageSubmitted, [&] { ++submissions; });
    controller.openStartup({"ReliefForge", filename});
    RF_REQUIRE(controller.activeExampleId().isEmpty());
    RF_REQUIRE(controller.sourceName() == "own-image.png");
    RF_REQUIRE(controller.sourceInfo().startsWith("7 × 5"));
    RF_REQUIRE(submissions == 1);
    RF_REQUIRE(controller.loadExample("radial-dome"));
    controller.openImage(QUrl::fromLocalFile(filename));
    RF_REQUIRE(controller.activeExampleId().isEmpty());
    const auto invalid = temporary.filePath("invalid.png");
    QFile file(invalid);
    RF_REQUIRE(file.open(QIODevice::WriteOnly));
    file.write("not an image");
    file.close();
    rf::app::AppController failed;
    failed.openStartup({"ReliefForge", invalid});
    RF_REQUIRE(!failed.hasImage());
    RF_REQUIRE(!failed.errorText().isEmpty());
}

RF_TEST("Height graph samples the exact active curve for every preset and selection change") {
    rf::app::AppController controller;
    int changes = 0;
    QObject::connect(&controller, &rf::app::AppController::parametersChanged, [&] { ++changes; });
    const std::array curves{rf::HeightCurve::linear(), rf::HeightCurve::soft(),
        rf::HeightCurve::strong(), rf::HeightCurve::basRelief(), rf::HeightCurve::highRelief()};
    for (int i = 0; i < static_cast<int>(curves.size()); ++i) {
        controller.setCurvePreset(i);
        RF_REQUIRE(controller.curvePreset() == i);
        requireCurveSamples(controller, curves[i]);
    }
    RF_REQUIRE(changes == 4);
    controller.setCurvePreset(5);
    requireCurveSamples(controller, curves.back());
    controller.setCurvePreset(0);
    requireCurveSamples(controller, rf::HeightCurve::linear());
    RF_REQUIRE(changes == 6);
    controller.setCurvePreset(-1);
    controller.setCurvePreset(6);
    RF_REQUIRE(changes == 6);
}

RF_TEST("Reopened custom curves drive the graph and saved settings override new-app defaults") {
    QTemporaryDir temporary;
    RF_REQUIRE(temporary.isValid());
    QImage image(2, 2, QImage::Format_RGB32);
    image.fill(Qt::white);
    RF_REQUIRE(image.save(temporary.filePath("source.png")));
    rf::ProjectDocument project;
    project.sourceImage = "source.png";
    project.parameters.image.blurRadius = 1.25;
    project.parameters.resolution.preset = rf::ResolutionPreset::Draft;
    project.parameters.height.curve = rf::HeightCurve({{0, 0.2}, {0.3, 0.8}, {1, 0.9}});
    const auto projectPath = temporary.filePath("custom.reliefstudio");
    RF_REQUIRE(rf::succeeded(rf::ProjectSerializer::save(project, projectPath.toStdString())));
    rf::app::AppController controller;
    controller.openProject(QUrl::fromLocalFile(projectPath));
    RF_REQUIRE(controller.errorText().isEmpty());
    RF_REQUIRE(controller.hasImage());
    RF_REQUIRE(controller.curvePreset() == 5);
    requireCurveSamples(controller, project.parameters.height.curve);
    RF_REQUIRE_NEAR(controller.blurRadius(), 1.25, 1.0e-12);
    RF_REQUIRE(controller.resolutionPreset() == static_cast<int>(rf::ResolutionPreset::Draft));
    // Do not pump the event loop: only graph/default restoration is under test,
    // not the deferred geometry rebuild.
}

RF_TEST("Mesh counts retain full integer precision with grouping and no scientific notation") {
    using rf::app::AppController;
    QLocale english(QLocale::English, QLocale::UnitedKingdom);
    english.setNumberOptions(QLocale::OmitGroupSeparator);
    RF_REQUIRE(AppController::formatMeshCount(0, english) == "0");
    RF_REQUIRE(AppController::formatMeshCount(999, english) == "999");
    RF_REQUIRE(AppController::formatMeshCount(1000, english) == "1,000");
    RF_REQUIRE(AppController::formatMeshCount(2347020, english) == "2,347,020");
    RF_REQUIRE(AppController::formatMeshCount(4294967296ULL, english) == "4,294,967,296");
    RF_REQUIRE(AppController::formatMeshCount(std::numeric_limits<qulonglong>::max(), english)
               == "18,446,744,073,709,551,615");
    RF_REQUIRE(AppController::formatMeshCount(2347020, QLocale(QLocale::German, QLocale::Germany))
               == "2.347.020");
}

RF_TEST("All material presets change appearance only and reject invalid selections") {
    rf::app::AppController controller;
    int materialsChanged = 0;
    int parametersChanged = 0;
    QObject::connect(&controller, &rf::app::AppController::previewMaterialChanged,
        [&] { ++materialsChanged; });
    QObject::connect(&controller, &rf::app::AppController::parametersChanged,
        [&] { ++parametersChanged; });
    std::set<QString> colors;
    const auto names = controller.previewMaterialNames();
    RF_REQUIRE(names.size() == 7);
    for (int i = 0; i < names.size(); ++i) {
        controller.setPreviewMaterial(i);
        RF_REQUIRE(controller.previewMaterial() == i);
        const auto properties = controller.previewMaterialProperties();
        const auto color = properties.value("baseColor").value<QColor>();
        RF_REQUIRE(color.isValid());
        colors.insert(color.name());
        for (const auto* key : {"roughness", "metalness", "specularAmount", "clearcoatAmount"}) {
            RF_REQUIRE(properties.contains(key));
            RF_REQUIRE(properties.value(key).toDouble() >= 0.0);
            RF_REQUIRE(properties.value(key).toDouble() <= 1.0);
        }
        RF_REQUIRE(!controller.busy());
    }
    RF_REQUIRE(colors.size() == 7);
    RF_REQUIRE(materialsChanged == 6);
    RF_REQUIRE(parametersChanged == 0);
    controller.setPreviewMaterial(-1);
    controller.setPreviewMaterial(names.size());
    controller.setPreviewMaterial(6);
    RF_REQUIRE(materialsChanged == 6);
    controller.setPreviewMaterial(0);
    RF_REQUIRE(materialsChanged == 7);
    RF_REQUIRE(parametersChanged == 0);
    RF_REQUIRE(!controller.busy());
}
