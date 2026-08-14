#pragma once

#include <filesystem>

namespace azurerender {

class IAssetImporter {
public:
    virtual ~IAssetImporter() = default;
    [[nodiscard]] virtual bool supports(
        const std::filesystem::path& path) const noexcept = 0;
};

}  // namespace azurerender
