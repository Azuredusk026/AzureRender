#pragma once
#include "scene/SceneDescription.hpp"
#include <functional>
#include <optional>
namespace azurerender {
struct SelectionBoundsResult {
    bool valid=false;
    scene::AxisAlignedBounds bounds;
    std::string diagnostic;
};
class SelectionBounds {
public:
    using Provider=std::function<std::optional<scene::AxisAlignedBounds>(const std::string&)>;
    static SelectionBoundsResult resolve(const scene::SceneDescription& document,
        const std::vector<std::string>& selection, const Provider& provider={}, float fallbackRadius=.5F);
};
}
