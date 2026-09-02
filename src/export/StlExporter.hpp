#pragma once

#include "core/Mesh.hpp"
#include "core/Types.hpp"

#include <filesystem>
#include <string>

namespace rf {

enum class StlEncoding {
    Binary,
    Ascii,
};

struct StlExportOptions {
    StlEncoding encoding{StlEncoding::Binary};
    std::string solidName{"ReliefForge relief"};
    bool requireManifold{true};
};

class StlExporter {
public:
    static Result<std::monostate> write(
        const Mesh& mesh,
        const std::filesystem::path& path,
        const StlExportOptions& options = {});
};

} // namespace rf

