#pragma once
#include "editor/EditorContext.hpp"
namespace azurerender {
// Reusable project assets live separately from scene-object history.
std::string ensureBuiltinGeometry(EditorContext& context,const std::string& kind,
                                 const std::array<float,3>& dimensions);
}
