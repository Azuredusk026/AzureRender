#pragma once
#include "runtime/Project.hpp"
#include "render/ReferenceCameraView.hpp"
#include "render/ScreenObjectVisibilityNumerics.hpp"
#include <memory>
namespace azurerender {
struct ProjectReferenceViews {
    std::uint32_t schemaVersion=1;
    std::map<std::string,ReferenceCameraView> views;
    ScreenObjectVisibilityArithmetic objectVisibilityArithmetic=ScreenObjectVisibilityArithmetic::Device;
    std::shared_ptr<const ScreenObjectVisibilityNumerics> objectVisibilityProfile;
};
nlohmann::json encodeProjectReferenceViews(const ProjectReferenceViews&);
ProjectReferenceViews decodeProjectReferenceViews(const nlohmann::json&);
std::filesystem::path projectReferenceViewsPath(const Project&);
std::filesystem::path projectReferenceVisibilityProfilePath(const Project&);
ProjectReferenceViews loadProjectReferenceViews(const Project&);
// Caller serializes writes for this project. Returned snapshots own all values.
void saveProjectReferenceViews(const Project&,const ProjectReferenceViews&);
}
