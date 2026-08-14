#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace azurerender {

class ResourceLocator final {
public:
    explicit ResourceLocator(std::filesystem::path explicitRoot = {});

    [[nodiscard]] std::filesystem::path shaderDirectory() const;
    [[nodiscard]] std::filesystem::path publicAsset(
        const std::filesystem::path& relative) const;
    [[nodiscard]] std::filesystem::path rampProfile() const;
    [[nodiscard]] std::filesystem::path rampAtlas() const;
    [[nodiscard]] std::filesystem::path captureDirectory() const;
    [[nodiscard]] std::filesystem::path resolveAsset(
        const std::filesystem::path& requested) const;
    [[nodiscard]] const std::vector<std::filesystem::path>& searchRoots() const noexcept {
        return roots_;
    }

private:
    std::vector<std::filesystem::path> roots_;
    [[nodiscard]] std::filesystem::path find(
        const std::filesystem::path& relative,
        const char* type) const;
};

}  // namespace azurerender
