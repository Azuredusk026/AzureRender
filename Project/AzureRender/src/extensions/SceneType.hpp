#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

namespace azurerender {

// Identifies which pluggable scene renderer owns the current frame. The enum
// is deliberately small and versioned: new scene types are added by registering
// an ISceneRenderer under a new name, and this enum only carries the renderers
// that ship inside the renderer core.
enum class SceneType : std::uint32_t {
    Character = 0,
    Blackhole = 1,
    Count = 2,
};

// Converts a CLI/scene document scene-type name into the enum. Unknown names
// throw so configuration errors surface at parse time instead of at render
// time.
[[nodiscard]] inline SceneType sceneTypeFromString(const std::string& name) {
    if (name == "character") {
        return SceneType::Character;
    }
    if (name == "blackhole") {
        return SceneType::Blackhole;
    }
    throw std::invalid_argument(
        "Unknown scene type: " + name
        + " (accepted: character, blackhole)");
}

[[nodiscard]] inline const char* sceneTypeName(const SceneType type) noexcept {
    switch (type) {
    case SceneType::Character:
        return "character";
    case SceneType::Blackhole:
        return "blackhole";
    default:
        return "unknown";
    }
}

}  // namespace azurerender
