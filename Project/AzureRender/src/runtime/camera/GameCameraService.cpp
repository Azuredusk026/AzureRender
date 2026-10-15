#include "runtime/camera/GameCameraService.hpp"
#include "runtime/RuntimeLifecycle.hpp"
#include "ecs/CameraComponents.hpp"
#include "render/CameraProjection.hpp"
#include "scene/SceneDescription.hpp"
#include <limits>
namespace azurerender {
CameraViewDescriptor gameCameraView(const game::GameCamera& camera,const std::array<float,16>& matrix){
    CameraViewDescriptor view;
        view.position={matrix[12],matrix[13],matrix[14]};
        const auto forward=internal::normalize({-matrix[8],-matrix[9],-matrix[10]});
        view.target=internal::addVectors(view.position,forward);
        view.up=internal::normalize({matrix[4],matrix[5],matrix[6]});
        view.projection=camera.orthographic?CameraProjectionKind::Orthographic:CameraProjectionKind::Perspective;
        view.fov=camera.fieldOfView*.017453292519943295F;
        view.orthoHeight=camera.orthographicHeight;view.nearPlane=camera.nearPlane;view.farPlane=camera.farPlane;
    validateCameraView(view,1.F);return view;
}
GameCameraSelection resolveGameCamera(RuntimeLifecycle& runtime,const SceneDocument& sceneDocument){
    GameCameraSelection result;std::int64_t priority=-1;
    const auto scene=sceneDocument.renderDescription();
    const auto matrices=scene::resolveNodeWorldTransforms(scene);
    for(std::size_t i=0;i<scene.nodes.size();++i){
        const auto& node=scene.nodes[i];
        const auto* camera=runtime.world().tryGet<game::GameCamera>(runtime.entity(node.id));
        if(!camera||!camera->enabled||camera->priority<priority
            ||(camera->priority==priority&&!result.node.empty()&&node.id>=result.node))continue;
        CameraViewDescriptor view;
        try{view=gameCameraView(*camera,matrices[i]);}catch(const std::exception& e){result.diagnostic=node.id+": "+e.what();continue;}
        result.view=view;result.node=node.id;priority=camera->priority;
    }
    if(!result.view&&result.diagnostic.empty())result.diagnostic="No enabled GameCamera";
    return result;
}
}
