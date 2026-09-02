#pragma once

#include "app/ReliefGeometry.hpp"
#include "core/Project.hpp"
#include "core/ReliefPipeline.hpp"

#include <QFutureWatcher>
#include <QLocale>
#include <QObject>
#include <QTimer>
#include <QUrl>
#include <QVariant>

#include <memory>

namespace rf::app {

class AppController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool hasImage READ hasImage NOTIFY stateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY stateChanged)
    Q_PROPERTY(QUrl sourceUrl READ sourceUrl NOTIFY stateChanged)
    Q_PROPERTY(QString sourceInfo READ sourceInfo NOTIFY stateChanged)
    Q_PROPERTY(QString sourceName READ sourceName NOTIFY stateChanged)
    Q_PROPERTY(QString activeExampleId READ activeExampleId NOTIFY stateChanged)
    Q_PROPERTY(QVariantList exampleCatalog READ exampleCatalog CONSTANT)
    Q_PROPERTY(QString statusText READ statusText NOTIFY stateChanged)
    Q_PROPERTY(QString errorText READ errorText NOTIFY stateChanged)
    Q_PROPERTY(double widthMm READ widthMm WRITE setWidthMm NOTIFY parametersChanged)
    Q_PROPERTY(double heightMm READ heightMm NOTIFY parametersChanged)
    Q_PROPERTY(double reliefDepthMm READ reliefDepthMm WRITE setReliefDepthMm NOTIFY parametersChanged)
    Q_PROPERTY(double baseThicknessMm READ baseThicknessMm WRITE setBaseThicknessMm NOTIFY parametersChanged)
    Q_PROPERTY(double contrast READ contrast WRITE setContrast NOTIFY parametersChanged)
    Q_PROPERTY(double gamma READ gamma WRITE setGamma NOTIFY parametersChanged)
    Q_PROPERTY(double blurRadius READ blurRadius WRITE setBlurRadius NOTIFY parametersChanged)
    Q_PROPERTY(bool inverted READ inverted WRITE setInverted NOTIFY parametersChanged)
    Q_PROPERTY(int reliefStyle READ reliefStyle WRITE setReliefStyle NOTIFY parametersChanged)
    Q_PROPERTY(int curvePreset READ curvePreset WRITE setCurvePreset NOTIFY parametersChanged)
    Q_PROPERTY(QVariantList heightCurveSamples READ heightCurveSamples NOTIFY parametersChanged)
    Q_PROPERTY(int resolutionPreset READ resolutionPreset WRITE setResolutionPreset NOTIFY parametersChanged)
    Q_PROPERTY(qulonglong vertexCount READ vertexCount NOTIFY stateChanged)
    Q_PROPERTY(qulonglong triangleCount READ triangleCount NOTIFY stateChanged)
    Q_PROPERTY(QString vertexCountText READ vertexCountText NOTIFY stateChanged)
    Q_PROPERTY(QString triangleCountText READ triangleCountText NOTIFY stateChanged)
    Q_PROPERTY(int previewMaterial READ previewMaterial WRITE setPreviewMaterial NOTIFY previewMaterialChanged)
    Q_PROPERTY(QStringList previewMaterialNames READ previewMaterialNames CONSTANT)
    Q_PROPERTY(QVariantMap previewMaterialProperties READ previewMaterialProperties NOTIFY previewMaterialChanged)
    Q_PROPERTY(bool meshValid READ meshValid NOTIFY stateChanged)
    Q_PROPERTY(ReliefGeometry* geometry READ geometry CONSTANT)

public:
    explicit AppController(QObject* parent = nullptr);

    [[nodiscard]] bool hasImage() const noexcept { return !source_.empty(); }
    [[nodiscard]] bool busy() const noexcept {
        return rebuildTimer_.isActive() || watcher_.isRunning() || printExportWatcher_.isRunning();
    }
    [[nodiscard]] QUrl sourceUrl() const noexcept { return sourceUrl_; }
    [[nodiscard]] QString sourceInfo() const;
    [[nodiscard]] QString sourceName() const;
    [[nodiscard]] QString activeExampleId() const { return activeExampleId_; }
    [[nodiscard]] QVariantList exampleCatalog() const;
    [[nodiscard]] QString statusText() const;
    [[nodiscard]] QString errorText() const noexcept { return errorText_; }
    [[nodiscard]] double widthMm() const noexcept;
    [[nodiscard]] double heightMm() const noexcept;
    [[nodiscard]] double reliefDepthMm() const noexcept;
    [[nodiscard]] double baseThicknessMm() const noexcept;
    [[nodiscard]] double contrast() const noexcept;
    [[nodiscard]] double gamma() const noexcept;
    [[nodiscard]] double blurRadius() const noexcept;
    [[nodiscard]] bool inverted() const noexcept;
    [[nodiscard]] int reliefStyle() const noexcept;
    [[nodiscard]] int curvePreset() const noexcept { return curvePreset_; }
    [[nodiscard]] QVariantList heightCurveSamples() const;
    [[nodiscard]] int resolutionPreset() const noexcept;
    [[nodiscard]] qulonglong vertexCount() const noexcept;
    [[nodiscard]] qulonglong triangleCount() const noexcept;
    [[nodiscard]] QString vertexCountText() const { return formatMeshCount(vertexCount()); }
    [[nodiscard]] QString triangleCountText() const { return formatMeshCount(triangleCount()); }
    [[nodiscard]] static QString formatMeshCount(qulonglong count, QLocale locale = QLocale());
    [[nodiscard]] int previewMaterial() const noexcept { return previewMaterial_; }
    [[nodiscard]] QStringList previewMaterialNames() const;
    [[nodiscard]] QVariantMap previewMaterialProperties() const;
    [[nodiscard]] bool meshValid() const noexcept;
    [[nodiscard]] ReliefGeometry* geometry() noexcept { return &geometry_; }

    void setWidthMm(double value);
    void setReliefDepthMm(double value);
    void setBaseThicknessMm(double value);
    void setContrast(double value);
    void setGamma(double value);
    void setBlurRadius(double value);
    void setInverted(bool value);
    void setReliefStyle(int value);
    void setCurvePreset(int value);
    void setResolutionPreset(int value);
    void setPreviewMaterial(int value);

    Q_INVOKABLE void openImage(const QUrl& url);
    Q_INVOKABLE bool loadExample(const QString& id);
    void openStartup(const QStringList& arguments);
    Q_INVOKABLE void openProject(const QUrl& url);
    Q_INVOKABLE void exportStl(const QUrl& url);
    Q_INVOKABLE void exportSmoothStl(const QUrl& url);
    Q_INVOKABLE void exportSvg(const QUrl& url);
    Q_INVOKABLE void exportDxf(const QUrl& url);
    Q_INVOKABLE void exportStep(const QUrl& url);
    Q_INVOKABLE void saveProject(const QUrl& url);
    Q_INVOKABLE void clearError();
    Q_INVOKABLE QString legalDocument(const QString& name) const;

signals:
    void stateChanged();
    void parametersChanged();
    void previewMaterialChanged();
    void imageSubmitted();
    void exportCompleted(const QString& path);

private:
    struct PrintExportOutcome {
        QString path;
        QString error;
    };

    void scheduleRebuild();
    void startRebuild();
    void showError(QString message);
    bool loadImage(const QUrl& url, const QString& exampleId = {});
    [[nodiscard]] std::filesystem::path localPath(const QUrl& url) const;
    [[nodiscard]] const MeshStatistics& previewStatistics() const noexcept;

    GrayImage source_;
    QUrl sourceUrl_;
    QString activeExampleId_;
    PipelineParameters parameters_;
    std::shared_ptr<const PipelineResult> result_;
    ReliefGeometry geometry_;
    QTimer rebuildTimer_;
    QFutureWatcher<std::shared_ptr<const PipelineResult>> watcher_;
    QFutureWatcher<PrintExportOutcome> printExportWatcher_;
    std::size_t requestedRevision_{};
    std::size_t runningRevision_{};
    QString errorText_;
    int curvePreset_{};
    int previewMaterial_{};
};

} // namespace rf::app
