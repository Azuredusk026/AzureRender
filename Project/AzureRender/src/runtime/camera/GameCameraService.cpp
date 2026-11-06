#include "runtime/camera/GameCameraService.hpp"
#include "runtime/RuntimeLifecycle.hpp"
#include "ecs/CameraComponents.hpp"
#include "render/CameraProjection.hpp"
#include "scene/SceneDescription.hpp"
#include <limits>
namespace azurerender {
namespace {
// Resolve only camera ancestry. A reachable cycle uses the complete scene's
// traversal order, which defines the existing deterministic cycle fallback.
class CameraWorldMatrices {
public:
    explicit CameraWorldMatrices(const scene::SceneDescription& scene)
        : scene_(scene), matrices_(scene.nodes.size()), state_(scene.nodes.size(),0) {
        indices_.reserve(scene.nodes.size());
        for(std::size_t index=0;index<scene.nodes.size();++index)
            if(!scene.nodes[index].id.empty())indices_.emplace(scene.nodes[index].id,index);
    }
    internal::Matrix4 resolve(std::size_t index) {
        if(state_[index]==2)return matrices_[index];
        const auto& node=scene_.nodes[index];
        const auto local=scene::composeTrs(node.translation,node.rotation,node.scale);
        if(state_[index]==1){cyclic_=true;state_[index]=2;return matrices_[index]=local;}
        state_[index]=1;
        auto world=local;
        const auto parent=indices_.find(node.parentId);
        if(!node.parentId.empty()&&parent!=indices_.end()&&parent->second!=index)
            world=internal::multiply(resolve(parent->second),local);
        state_[index]=2;
        return matrices_[index]=world;
    }
    bool cyclic() const noexcept{return cyclic_;}
    const internal::Matrix4& at(std::size_t index) const{return matrices_[index];}
private:
    const scene::SceneDescription& scene_;
    std::unordered_map<std::string,std::size_t> indices_;
    std::vector<internal::Matrix4> matrices_;
    std::vector<std::uint8_t> state_;
    bool cyclic_=false;
};
}
CameraViewDescriptor gameCameraView(const game::GameCamera& camera,const std::array<float,16>& matrix){
    CameraViewDescriptor view;
        view.position={matrix[12],matrix[13],matrix[14]};
        const auto forward=internal::normalize({-matrix[8],-matrix[9],-matrix[10]});
        view.target=internal::addVectors(view.position,forward);
        view.up=internal::normalize({matrix[4],matrix[5],matrix[6]});
        view.projection=camera.orthographic?CameraProjectionKind::Orthographic:CameraProjectionKind::Perspective;
        view.fov=camera.fieldOfView*.017453292519943295F;
        view.orthoHeight=camera.orthographicHeight;view.nearPlane=camera.nearPlane;view.farPlane=camera.farPlane;
        view.projectionOffsetX=camera.projectionOffsetX;view.projectionOffsetY=camera.projectionOffsetY;
    validateCameraView(view,1.F);return view;
}
GameCameraSelection resolveGameCamera(RuntimeLifecycle& runtime,const SceneDocument& sceneDocument){
    return resolveGameCamera(runtime,sceneDocument.renderDescription());
}
GameCameraSelection resolveGameCamera(RuntimeLifecycle& runtime,const scene::SceneDescription& scene){
    GameCameraSelection result;std::int64_t priority=-1;
    const auto* cameras=runtime.world().findComponentArray<game::GameCamera>();
    std::unordered_map<std::string,const game::GameCamera*> byNode;
    if(cameras) {
        byNode.reserve(cameras->size());
        for(std::size_t index=0;index<cameras->size();++index) {
            const auto& camera=cameras->dense()[index];
            if(!camera.enabled)continue;
            auto id=runtime.nodeId(cameras->entities()[index]);
            if(!id.empty())byNode.emplace(std::move(id),&camera);
        }
    }
    if(byNode.empty()){result.diagnostic="No enabled GameCamera";return result;}
    std::vector<std::pair<std::size_t,const game::GameCamera*>> candidates;
    for(std::size_t index=0;index<scene.nodes.size();++index) {
        const auto found=byNode.find(scene.nodes[index].id);
        if(found!=byNode.end())candidates.emplace_back(index,found->second);
    }
    if(candidates.empty()){result.diagnostic="No enabled GameCamera";return result;}
    CameraWorldMatrices prepared(scene);
    for(const auto& candidate:candidates)static_cast<void>(prepared.resolve(candidate.first));
    const auto fallback=prepared.cyclic()?scene::resolveNodeWorldTransforms(scene):std::vector<internal::Matrix4>{};
    for(const auto& [index,camera]:candidates){
        const auto& node=scene.nodes[index];
        if(camera->priority<priority
            ||(camera->priority==priority&&!result.node.empty()&&node.id>=result.node))continue;
        CameraViewDescriptor view;
        try{view=gameCameraView(*camera,fallback.empty()?prepared.at(index):fallback[index]);}catch(const std::exception& e){result.diagnostic=node.id+": "+e.what();continue;}
        result.view=view;result.node=node.id;priority=camera->priority;
    }
    if(!result.view&&result.diagnostic.empty())result.diagnostic="No enabled GameCamera";
    return result;
}
}
