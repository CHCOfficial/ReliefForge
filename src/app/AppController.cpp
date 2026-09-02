#include "app/AppController.hpp"

#include "export/StlExporter.hpp"
#include "export/VectorExporter.hpp"
#ifdef RELIEFFORGE_HAS_OCCT
#include "cad/StepExporter.hpp"
#endif

#include <QImage>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QImageReader>
#include <QColor>
#include <QLocale>
#include <QtConcurrentRun>

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace rf::app {
namespace {

struct MaterialPreset {
    const char* name;
    const char* color;
    double roughness;
    double metalness;
    double specular;
    double clearcoat;
};

constexpr std::array<MaterialPreset, 7> MaterialPresets{{
    {"Clay", "#c7c2b8", 0.78, 0.02, 0.5, 0.0},
    {"Matte White", "#f1f1ec", 0.94, 0.0, 0.3, 0.0},
    {"Aluminium", "#c5ced8", 0.28, 0.8, 0.5, 0.0},
    {"Bronze", "#b77b43", 0.38, 0.75, 0.5, 0.0},
    {"Dark Metal", "#414956", 0.32, 0.7, 0.5, 0.0},
    {"Wood", "#9c6134", 0.7, 0.0, 0.3, 0.0},
    {"Resin", "#718cba", 0.18, 0.0, 0.7, 1.0},
}};

} // namespace

AppController::AppController(QObject* parent) : QObject(parent) {
    parameters_.image.blurRadius = 0.0;
    parameters_.resolution.preset = ResolutionPreset::Ultra;
    rebuildTimer_.setSingleShot(true);
    rebuildTimer_.setInterval(80);
    connect(&rebuildTimer_, &QTimer::timeout, this, &AppController::startRebuild);
    connect(&geometry_, &ReliefGeometry::smoothShadingChanged, this, &AppController::stateChanged);
    connect(&watcher_, &QFutureWatcher<std::shared_ptr<const PipelineResult>>::finished, this, [this] {
        try {
            auto completed = watcher_.result();
            if (runningRevision_ == requestedRevision_) {
                result_ = std::move(completed);
                geometry_.setMeshes(
                    std::shared_ptr<const Mesh>(result_, &result_->mesh), result_->smoothPrintMesh);
                errorText_.clear();
            }
        } catch (const std::exception& error) {
            showError(QString::fromUtf8(error.what()));
        }
        if (runningRevision_ != requestedRevision_) {
            rebuildTimer_.start(0);
        }
        emit stateChanged();
    });
    connect(&printExportWatcher_, &QFutureWatcher<PrintExportOutcome>::finished, this, [this] {
        const auto outcome = printExportWatcher_.result();
        if (!outcome.error.isEmpty()) {
            showError(outcome.error);
            return;
        }
        errorText_.clear();
        emit exportCompleted(outcome.path);
        emit stateChanged();
    });
}

QString AppController::sourceInfo() const {
    if (source_.empty()) return {};
    return QStringLiteral("%1 × %2 px · sRGB luminance")
        .arg(source_.width()).arg(source_.height());
}

QVariantList AppController::exampleCatalog() const {
    static const QVariantList catalog = [] {
        QFile file(QStringLiteral(":/examples/catalog.json"));
        if (!file.open(QIODevice::ReadOnly)) return QVariantList{};
        auto entries = QJsonDocument::fromJson(file.readAll()).array().toVariantList();
        for (auto& entry : entries) {
            auto item = entry.toMap();
            item.insert("imageUrl", QUrl(QStringLiteral("qrc:/examples/%1.png").arg(item.value("id").toString())));
            entry = item;
        }
        return entries;
    }();
    return catalog;
}

QString AppController::sourceName() const {
    for (const auto& entry : exampleCatalog()) {
        const auto item = entry.toMap();
        if (item.value("id").toString() == activeExampleId_) return item.value("name").toString();
    }
    return QFileInfo(sourceUrl_.toLocalFile()).fileName();
}

bool AppController::loadExample(const QString& id) {
    for (const auto& entry : exampleCatalog()) {
        const auto item = entry.toMap();
        if (item.value("id").toString() == id) return loadImage(item.value("imageUrl").toUrl(), id);
    }
    showError(QStringLiteral("This built-in example is not available in this version."));
    return false;
}

void AppController::openStartup(const QStringList& arguments) {
    for (qsizetype index = 1; index < arguments.size(); ++index) {
        const auto& argument = arguments.at(index);
        if (argument.startsWith(QStringLiteral("-psn_"))) continue;
        const QFileInfo file(argument);
        if (!file.exists() || !file.isFile()) continue;
        const auto url = QUrl::fromLocalFile(file.absoluteFilePath());
        if (argument.endsWith(QStringLiteral(".reliefstudio"), Qt::CaseInsensitive)) openProject(url);
        else openImage(url);
        return; // Explicit files take priority, even if decoding fails.
    }
    loadExample(QStringLiteral("topographic-waves"));
}

QString AppController::statusText() const {
    if (rebuildTimer_.isActive() || watcher_.isRunning()) return QStringLiteral("Building print-ready previews…");
    if (printExportWatcher_.isRunning()) return QStringLiteral("Writing smooth high-resolution STL…");
    if (!result_) return hasImage() ? QStringLiteral("Preparing relief…") : QStringLiteral("Ready");
    const auto& dimensions = result_->heightField.dimensions();
    return QStringLiteral("%1 × %2 × %3 mm  |  %4 triangles  |  %5  |  %6")
        .arg(dimensions.widthMm, 0, 'f', 2)
        .arg(dimensions.heightMm, 0, 'f', 2)
        .arg(result_->heightField.maximumZ(), 0, 'f', 2)
        .arg(triangleCountText())
        .arg(meshValid() ? QStringLiteral("Manifold") : QStringLiteral("Needs repair"))
        .arg(geometry_.smoothShading() ? QStringLiteral("Smooth print") : QStringLiteral("Original geometry"));
}

double AppController::widthMm() const noexcept { return parameters_.height.dimensions.widthMm; }
double AppController::heightMm() const noexcept { return parameters_.height.dimensions.heightMm; }
double AppController::reliefDepthMm() const noexcept { return parameters_.height.dimensions.reliefDepthMm; }
double AppController::baseThicknessMm() const noexcept { return parameters_.height.dimensions.baseThicknessMm; }
double AppController::contrast() const noexcept { return parameters_.image.contrast; }
double AppController::gamma() const noexcept { return parameters_.image.gamma; }
double AppController::blurRadius() const noexcept { return parameters_.image.blurRadius; }
bool AppController::inverted() const noexcept { return parameters_.image.invert; }
int AppController::reliefStyle() const noexcept { return static_cast<int>(parameters_.height.style); }
int AppController::resolutionPreset() const noexcept { return static_cast<int>(parameters_.resolution.preset); }
const MeshStatistics& AppController::previewStatistics() const noexcept {
    static const MeshStatistics empty;
    if (!result_) return empty;
    return geometry_.smoothShading() ? result_->smoothPrintStatistics : result_->statistics;
}
qulonglong AppController::vertexCount() const noexcept { return previewStatistics().vertices; }
qulonglong AppController::triangleCount() const noexcept { return previewStatistics().triangles; }
bool AppController::meshValid() const noexcept { return result_ && previewStatistics().manifold(); }

QString AppController::formatMeshCount(qulonglong count, QLocale locale) {
    // Format the integer before it reaches QML: no floating-point conversion,
    // scientific notation, or loss of precision for very large meshes.
    locale.setNumberOptions(locale.numberOptions() & ~QLocale::OmitGroupSeparator);
    return locale.toString(count);
}

QVariantList AppController::heightCurveSamples() const {
    QVariantList samples;
    constexpr int intervals = 256;
    samples.reserve(intervals + 1);
    for (int i = 0; i <= intervals; ++i) {
        const double x = static_cast<double>(i) / intervals;
        samples.append(QVariantMap{{"x", x}, {"y", parameters_.height.curve.evaluate(x)}});
    }
    return samples;
}

QStringList AppController::previewMaterialNames() const {
    QStringList names;
    for (const auto& preset : MaterialPresets) names.append(QString::fromLatin1(preset.name));
    return names;
}

QVariantMap AppController::previewMaterialProperties() const {
    const auto& preset = MaterialPresets[previewMaterial_];
    return {{"baseColor", QColor(QString::fromLatin1(preset.color))},
            {"roughness", preset.roughness}, {"metalness", preset.metalness},
            {"specularAmount", preset.specular}, {"clearcoatAmount", preset.clearcoat}};
}

void AppController::setPreviewMaterial(int value) {
    if (value < 0 || value >= static_cast<int>(MaterialPresets.size()) || value == previewMaterial_) return;
    previewMaterial_ = value;
    // Appearance is independent of the printable surface and never rebuilds it.
    emit previewMaterialChanged();
}

void AppController::setWidthMm(double value) {
    if (value <= 0.0 || qFuzzyCompare(value, widthMm())) return;
    const auto aspect = source_.empty() ? 1.0 : source_.height() / static_cast<double>(source_.width());
    parameters_.height.dimensions.widthMm = value;
    parameters_.height.dimensions.heightMm = value * aspect;
    emit parametersChanged(); scheduleRebuild();
}

void AppController::setReliefDepthMm(double value) {
    if (value < 0.0 || qFuzzyCompare(value, reliefDepthMm())) return;
    parameters_.height.dimensions.reliefDepthMm = value;
    emit parametersChanged(); scheduleRebuild();
}

void AppController::setBaseThicknessMm(double value) {
    if (value <= 0.0 || qFuzzyCompare(value, baseThicknessMm())) return;
    parameters_.height.dimensions.baseThicknessMm = value;
    emit parametersChanged(); scheduleRebuild();
}

void AppController::setContrast(double value) {
    if (qFuzzyCompare(value, contrast())) return;
    parameters_.image.contrast = value;
    emit parametersChanged(); scheduleRebuild();
}

void AppController::setGamma(double value) {
    if (value <= 0.0 || qFuzzyCompare(value, gamma())) return;
    parameters_.image.gamma = value;
    emit parametersChanged(); scheduleRebuild();
}

void AppController::setBlurRadius(double value) {
    if (value < 0.0 || qFuzzyCompare(value, blurRadius())) return;
    parameters_.image.blurRadius = value;
    emit parametersChanged(); scheduleRebuild();
}

void AppController::setInverted(bool value) {
    if (value == inverted()) return;
    parameters_.image.invert = value;
    emit parametersChanged(); scheduleRebuild();
}

void AppController::setReliefStyle(int value) {
    if (value < 0 || value > static_cast<int>(ReliefStyle::Edge) || value == reliefStyle()) return;
    parameters_.height.style = static_cast<ReliefStyle>(value);
    emit parametersChanged(); scheduleRebuild();
}

void AppController::setCurvePreset(int value) {
    if (value < 0 || value > 5 || value == curvePreset_) return;
    curvePreset_ = value;
    switch (value) {
    case 0: parameters_.height.curve = HeightCurve::linear(); break;
    case 1: parameters_.height.curve = HeightCurve::soft(); break;
    case 2: parameters_.height.curve = HeightCurve::strong(); break;
    case 3: parameters_.height.curve = HeightCurve::basRelief(); break;
    case 4: parameters_.height.curve = HeightCurve::highRelief(); break;
    case 5: emit parametersChanged(); return;
    default: return;
    }
    emit parametersChanged(); scheduleRebuild();
}

void AppController::setResolutionPreset(int value) {
    if (value < 0 || value > static_cast<int>(ResolutionPreset::Custom) || value == resolutionPreset()) return;
    parameters_.resolution.preset = static_cast<ResolutionPreset>(value);
    emit parametersChanged(); scheduleRebuild();
}

void AppController::openImage(const QUrl& url) {
    if (!url.isLocalFile()) return showError(QStringLiteral("Choose an image file on your computer."));
    loadImage(url);
}

bool AppController::loadImage(const QUrl& url, const QString& exampleId) {
    QImageReader reader(exampleId.isEmpty() ? url.toLocalFile() : QStringLiteral(":") + url.path());
    reader.setAutoTransform(true);
    auto image = reader.read();
    if (image.isNull()) {
        showError(QStringLiteral("Could not open the image. %1").arg(reader.errorString()));
        return false;
    }
    image = image.convertToFormat(QImage::Format_RGBA8888);
    std::vector<RgbaPixel> pixels(static_cast<std::size_t>(image.width() * image.height()));
    for (int y = 0; y < image.height(); ++y) {
        const auto* row = image.constScanLine(y);
        for (int x = 0; x < image.width(); ++x) {
            const auto offset = x * 4;
            pixels[static_cast<std::size_t>(y * image.width() + x)] =
                {row[offset], row[offset + 1], row[offset + 2], row[offset + 3]};
        }
    }
    source_ = GrayImage::fromRgba(
        static_cast<std::size_t>(image.width()),
        static_cast<std::size_t>(image.height()),
        pixels,
        false);
    sourceUrl_ = url;
    activeExampleId_ = exampleId;
    if (!exampleId.isEmpty()) {
        parameters_ = PipelineParameters{};
        parameters_.resolution.preset = ResolutionPreset::Ultra;
        curvePreset_ = 0;
        setPreviewMaterial(0);
        geometry_.setSmoothShading(true);
    }
    parameters_.height.dimensions.heightMm = widthMm() * source_.height() / source_.width();
    result_.reset();
    geometry_.setMeshes({}, {});
    errorText_.clear();
    emit stateChanged();
    emit parametersChanged();
    scheduleRebuild();
    emit imageSubmitted();
    return true;
}

void AppController::openProject(const QUrl& url) {
    const auto loaded = ProjectSerializer::load(localPath(url));
    if (!succeeded(loaded)) {
        return showError(QString::fromStdString(std::get<Error>(loaded).message));
    }
    auto project = std::get<ProjectDocument>(loaded);
    auto sourcePath = project.sourceImage;
    const auto sourceReference = QString::fromStdString(sourcePath.generic_string());
    if (sourceReference.startsWith(QStringLiteral("builtin:"))) {
        if (!loadExample(sourceReference.mid(8))) return;
    } else {
        if (sourcePath.is_relative()) sourcePath = localPath(url).parent_path() / sourcePath;
        if (!loadImage(QUrl::fromLocalFile(QString::fromStdString(sourcePath.string())))) return;
    }
    parameters_ = std::move(project.parameters);
    curvePreset_ = 5;
    emit parametersChanged();
    scheduleRebuild();
}

void AppController::exportStl(const QUrl& url) {
    if (!result_ || busy()) return showError(QStringLiteral("Wait for the relief preview before exporting."));
    const auto exported = StlExporter::write(result_->mesh, localPath(url));
    if (!succeeded(exported)) return showError(QString::fromStdString(std::get<Error>(exported).message));
    emit exportCompleted(url.toLocalFile());
}

void AppController::exportSmoothStl(const QUrl& url) {
    if (!result_ || busy()) {
        return showError(QStringLiteral("Wait for the relief preview before exporting."));
    }

    // Capture the immutable mesh also held by ReliefGeometry. Export never
    // recomputes a different surface or depends on the current preview toggle.
    const auto smoothMesh = result_->smoothPrintMesh;
    const auto path = localPath(url);
    const auto displayPath = url.toLocalFile();
    printExportWatcher_.setFuture(QtConcurrent::run(
        [smoothMesh, path, displayPath]() -> PrintExportOutcome {
            try {
                if (!smoothMesh) return {{}, QStringLiteral("The smooth print preview is not ready.")};
                StlExportOptions options;
                options.solidName = "ReliefForge smooth high-resolution print";
                const auto exported = StlExporter::write(*smoothMesh, path, options);
                if (!succeeded(exported)) {
                    return {
                        {},
                        QString::fromStdString(std::get<Error>(exported).message),
                    };
                }
                return {displayPath, {}};
            } catch (const std::exception& error) {
                return {{}, QString::fromUtf8(error.what())};
            }
        }));
    emit stateChanged();
}

void AppController::exportSvg(const QUrl& url) {
    if (!result_ || busy()) return showError(QStringLiteral("Wait for the relief preview before exporting."));
    const auto vectors = ContourGenerator::contours(result_->heightField);
    const auto exported = VectorExporter::writeSvg(vectors, localPath(url));
    if (!succeeded(exported)) return showError(QString::fromStdString(std::get<Error>(exported).message));
    emit exportCompleted(url.toLocalFile());
}

void AppController::exportDxf(const QUrl& url) {
    if (!result_ || busy()) return showError(QStringLiteral("Wait for the relief preview before exporting."));
    const auto vectors = ContourGenerator::contours(result_->heightField);
    const auto exported = VectorExporter::writeDxf(vectors, localPath(url));
    if (!succeeded(exported)) return showError(QString::fromStdString(std::get<Error>(exported).message));
    emit exportCompleted(url.toLocalFile());
}

void AppController::exportStep(const QUrl& url) {
    if (!result_ || busy()) return showError(QStringLiteral("Wait for the relief preview before exporting."));
#ifdef RELIEFFORGE_HAS_OCCT
    const auto exported = StepExporter::write(result_->heightField, localPath(url));
    if (!succeeded(exported)) return showError(QString::fromStdString(std::get<Error>(exported).message));
    emit exportCompleted(url.toLocalFile());
#else
    Q_UNUSED(url)
    showError(QStringLiteral("This build does not include OpenCascade, so genuine STEP export is unavailable."));
#endif
}

void AppController::saveProject(const QUrl& url) {
    if (!hasImage()) return showError(QStringLiteral("Open an image or example before saving a project."));
    ProjectDocument project;
    project.sourceImage = activeExampleId_.isEmpty() ? localPath(sourceUrl_)
        : std::filesystem::path((QStringLiteral("builtin:") + activeExampleId_).toStdString());
    project.parameters = parameters_;
    const auto saved = ProjectSerializer::save(project, localPath(url));
    if (!succeeded(saved)) return showError(QString::fromStdString(std::get<Error>(saved).message));
    emit exportCompleted(url.toLocalFile());
}

void AppController::clearError() { errorText_.clear(); emit stateChanged(); }

QString AppController::legalDocument(const QString& name) const {
    const QStringList documents{"COPYING", "CREDITS.md", "THIRD_PARTY_NOTICES.md", "CORRESPONDING_SOURCE.md"};
    if (!documents.contains(name)) return {};
    QFile file(QStringLiteral(":/legal/") + name);
    if (!file.open(QIODevice::ReadOnly)) return QStringLiteral("See the legal documents included with your app download.");
    return QString::fromUtf8(file.readAll());
}

void AppController::scheduleRebuild() {
    if (source_.empty()) return;
    ++requestedRevision_;
    rebuildTimer_.start();
    emit stateChanged();
}

void AppController::startRebuild() {
    if (source_.empty() || watcher_.isRunning()) return;
    runningRevision_ = requestedRevision_;
    const auto source = source_;
    const auto parameters = parameters_;
    watcher_.setFuture(QtConcurrent::run([source, parameters] {
        return std::make_shared<const PipelineResult>(ReliefPipeline::build(source, parameters, true));
    }));
    emit stateChanged();
}

void AppController::showError(QString message) {
    errorText_ = std::move(message);
    emit stateChanged();
}

std::filesystem::path AppController::localPath(const QUrl& url) const {
    return std::filesystem::path(url.toLocalFile().toStdString());
}

} // namespace rf::app
