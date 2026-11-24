#pragma once
#include "editor/EditorContext.hpp"
#include "editor/commands/EditContracts.hpp"
#include "scene/TransformMath.hpp"
namespace azurerender {
struct GizmoDragRequest { std::string space="world",pivot="active"; };
struct GizmoDragUpdate { internal::Matrix4 matrix{}; };
class GizmoController {
public:
    explicit GizmoController(EditorContext& context):context_(context){}
    internal::Matrix4 pivotMatrix(const GizmoDragRequest& request) const;
    nlohmann::json begin(const GizmoDragRequest& request);
    void update(const GizmoDragUpdate& update);
    void commit();
    void cancel();
    bool active() const noexcept{return active_;}
private:
    EditorContext& context_;
    EditorContext::Snapshot initial_;
    std::vector<EditorContext::Snapshot> undoBefore_,redoBefore_;
    std::vector<internal::Matrix4> startWorld_;
    std::vector<std::size_t> roots_;
    internal::Matrix4 startPivot_{};
    DocumentVersion expected_;
    std::string mergeKey_;
    bool active_=false;
    bool updated_=false;
    void checkVersion() const;
};
}
