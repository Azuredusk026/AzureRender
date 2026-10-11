#pragma once

#include <filesystem>
#include "assets/AssetFormatRegistry.hpp"

namespace azurerender {

class IAssetImporter {
public:
    virtual ~IAssetImporter() = default;
    [[nodiscard]] virtual AssetDomain domain() const noexcept { return AssetDomain::Model; }
    [[nodiscard]] virtual std::vector<std::string> formatIds() const { return {}; }
    [[nodiscard]] virtual bool supports(
        const std::filesystem::path& path) const noexcept = 0;
};

}  // namespace azurerender
