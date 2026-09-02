#pragma once

#include "core/ReliefPipeline.hpp"
#include "core/Types.hpp"

#include <filesystem>
#include <string>

namespace rf {

inline constexpr unsigned CurrentProjectSchema = 1;

struct ProjectDocument {
    unsigned schemaVersion{CurrentProjectSchema};
    std::string applicationVersion{"1.0.0"};
    std::filesystem::path sourceImage;
    std::filesystem::path depthMap;
    PipelineParameters parameters;
    double cameraYawDegrees{-35.0};
    double cameraPitchDegrees{-28.0};
    double cameraDistance{180.0};
};

class ProjectSerializer {
public:
    static Result<std::monostate> save(
        const ProjectDocument& project,
        const std::filesystem::path& path);
    static Result<ProjectDocument> load(const std::filesystem::path& path);
};

} // namespace rf
