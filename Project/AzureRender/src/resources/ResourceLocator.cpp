#include "ResourceLocator.hpp"

#include "diagnostics/RuntimeDiagnostics.hpp"

#include <cstdlib>
#include <algorithm>
#include <stdexcept>

#ifdef _WIN32
#include <windows.h>
#endif

namespace azurerender {

namespace {

std::filesystem::path executableDirectory() {
#ifdef _WIN32
    std::wstring buffer(32768, L'\0');
    const DWORD length = GetModuleFileNameW(
        nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length != 0) {
        buffer.resize(length);
        return std::filesystem::path(buffer).parent_path();
    }
#else
    std::error_code error;
    const auto path = std::filesystem::read_symlink("/proc/self/exe", error);
    if (!error) {
        return path.parent_path();
    }
#endif
    return std::filesystem::current_path();
}

void appendUnique(
    std::vector<std::filesystem::path>& roots,
    std::filesystem::path root) {
    if (root.empty()) return;
    root = std::filesystem::weakly_canonical(root);
    if (std::find(roots.begin(), roots.end(), root) == roots.end()) {
        roots.push_back(std::move(root));
    }
}

}  // namespace

ResourceLocator::ResourceLocator(std::filesystem::path explicitRoot) {
    if (explicitRoot.empty()) {
        if (const char* environment = std::getenv("AZURERENDER_RESOURCE_ROOT")) {
            explicitRoot = environment;
        }
    }
    if (!explicitRoot.empty()) {
        appendUnique(roots_, std::move(explicitRoot));
    }
    const auto executable = executableDirectory();
    appendUnique(roots_, executable);
    appendUnique(roots_, executable / "../share/AzureRender");
#ifdef AZURERENDER_ASSET_DIR
    appendUnique(
        roots_, std::filesystem::path(AZURERENDER_ASSET_DIR).parent_path());
#endif
    appendUnique(roots_, std::filesystem::current_path());
}

std::filesystem::path ResourceLocator::find(
    const std::filesystem::path& relative,
    const char* type) const {
    for (const auto& root : roots_) {
        const auto candidate = root / relative;
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
    }
    std::string searched;
    for (const auto& root : roots_) {
        searched += "\n  " + (root / relative).string();
    }
    throw std::runtime_error(
        std::string("Missing ") + type + " resource '" + relative.string()
        + "'; searched:" + searched);
}

std::filesystem::path ResourceLocator::shaderDirectory() const {
    return find("shaders", "shader directory");
}

std::filesystem::path ResourceLocator::publicAsset(
    const std::filesystem::path& relative) const {
    return find(std::filesystem::path("assets_public") / relative, "asset");
}

std::filesystem::path ResourceLocator::rampProfile() const {
    return publicAsset("toon_ramp_profiles.json");
}

std::filesystem::path ResourceLocator::rampAtlas() const {
    return publicAsset("toon_ramp_atlas.ppm");
}

std::filesystem::path ResourceLocator::captureDirectory() const {
    for (const auto& root : roots_) {
        const auto candidate = root / "captures";
        if (std::filesystem::exists(candidate) || root == roots_.front()) {
            return candidate;
        }
    }
    return std::filesystem::current_path() / "captures";
}

std::filesystem::path ResourceLocator::resolveAsset(
    const std::filesystem::path& requested) const {
    if (requested.empty()) {
        return publicAsset("test_model.gltf");
    }
    if (requested.is_absolute() && std::filesystem::exists(requested)) {
        return requested;
    }
    if (std::filesystem::exists(requested)) {
        return std::filesystem::weakly_canonical(requested);
    }
    return publicAsset(requested);
}

}  // namespace azurerender
