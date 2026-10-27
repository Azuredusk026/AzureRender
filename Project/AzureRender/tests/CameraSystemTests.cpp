#include "runtime/ComponentRegistry.hpp"
#include "runtime/camera/GameCameraService.hpp"
#include "runtime/RuntimeLifecycle.hpp"
#include "ecs/CameraComponents.hpp"
#include "render/RenderCamera.hpp"
#include "scene/TransformMath.hpp"
#include <iostream>
using namespace azurerender;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(){try{
    auto registry=makeRuntimeComponentRegistry();
    const auto camera=registry.defaults("azure.camera");registry.validate("azure.camera",camera);
    check(camera.at("version")==2,"Projection offsets require camera component version two");
    for(const auto& property:registry.metadata().type("azure.camera").properties)
        if(property.name=="projectionOffsetX"||property.name=="projectionOffsetY")
            check(property.precision==8&&property.unit=="NDC","Subpixel offsets must retain adequate editor precision and an explicit unit");
    auto shifted=camera;shifted["data"]["projectionOffsetX"]=.25;shifted["data"]["projectionOffsetY"]=-.5;
    registry.validate("azure.camera",shifted);
    for(const auto& bad:std::vector<nlohmann::json>{true,nullptr,"0",2.00000001,-2.00000001}) {
        auto invalidOffset=shifted;invalidOffset["data"]["projectionOffsetX"]=bad;bool failed=false;
        try{registry.validate("azure.camera",invalidOffset);}catch(const std::exception&){failed=true;}
        check(failed,"Projection offsets must reject invalid types and out-of-range doubles");
    }
    auto legacy=camera;legacy["version"]=1;legacy["data"].erase("projectionOffsetX");legacy["data"].erase("projectionOffsetY");
    game::GameCamera migrated;registry.metadata().decode("azure.camera",&migrated,legacy);
    check(migrated.projectionOffsetX==0&&migrated.projectionOffsetY==0,"Version one cameras migrate to zero projection offsets");
    game::GameCamera offsetCamera;offsetCamera.projectionOffsetX=.25F;offsetCamera.projectionOffsetY=-.5F;
    const auto offsetView=gameCameraView(offsetCamera,scene::identityMatrix());
    check(offsetView.projectionOffsetX==.25F&&offsetView.projectionOffsetY==-.5F,"Game camera projection offsets reach the shared render view");
    game::GameCamera capturedCamera;capturedCamera.fieldOfView=57.22092455245762F;capturedCamera.farPlane=10000;
    capturedCamera.projectionOffsetX=-.0002929684706032276F;capturedCamera.projectionOffsetY=.00007717559492448345F;
    const auto capturedView=gameCameraView(capturedCamera,scene::composeTrs(
        {-.26604312658309937F,2.262058973312378F,-24.905324935913086F},
        {-176.57766118423316F,-.2822828552113573F,-179.98311892665737F},{1,1,1}));
    const std::array<float,3> capturedForward{.004926745314151049F,.059694841504096985F,.9982044696807861F};
    const std::array<float,3> capturedUp{-.00029462671955116093F,.9982166290283203F,-.05969411879777908F};
    for(unsigned axis=0;axis<3;++axis){
        check(std::abs(capturedView.target[axis]-capturedView.position[axis]-capturedForward[axis])<2e-6F,
            "Captured camera forward must follow the production X/Z angle convention");
        check(std::abs(capturedView.up[axis]-capturedUp[axis])<1e-6F,"Captured camera up reaches the shared render view");
    }
    check(capturedView.farPlane==10000,"Source camera clipping remains owned by the camera component");
    auto invalid=camera;invalid["data"]["nearPlane"]=100;invalid["data"]["farPlane"]=1;
    bool rejected=false;try{registry.validate("azure.camera",invalid);}catch(const std::exception&){rejected=true;}
    check(rejected,"Invalid clipping planes rejected");
    SceneDocument scene;scene.nodes={{"parent","Parent"},{"b","B","parent"},{"a","A"}};
    scene.nodes[0].translation={10,0,0};scene.nodes[1].translation={0,2,3};
    RuntimeLifecycle runtime;runtime.loadScene(scene);
    check(!resolveGameCamera(runtime,scene).view,"Missing camera is explicit");
    game::GameCamera component;component.priority=2;component.orthographic=true;
    runtime.world().addComponent(runtime.entity("b"),component);
    auto selected=resolveGameCamera(runtime,scene);
    check(selected.node=="b"&&selected.view->position==std::array<float,3>{10,2,3},"Game camera uses parent world transform");
    check(selected.view->target==std::array<float,3>{10,2,2}&&selected.view->projection==CameraProjectionKind::Orthographic,"Game camera uses local minus Z and authored lens");
    runtime.world().addComponent(runtime.entity("a"),component);
    check(resolveGameCamera(runtime,scene).node=="a","Equal priority uses stable identity");
    runtime.world().tryGet<game::GameCamera>(runtime.entity("a"))->enabled=false;
    check(resolveGameCamera(runtime,scene).node=="b","Disabled cameras are excluded");
    CameraViewDescriptor editing;editing.position={50,50,50};editing.target={0,0,0};
    const auto finalEdit=selectRenderCamera(editing,selected.view,{});
    check(finalEdit.source==RenderCameraSource::SceneView&&finalEdit.view.position==editing.position,"Simulation preserves SceneViewCamera");
    check(selectRenderCamera({},selected.view,{}).source==RenderCameraSource::GameCamera,"Play selects GameCamera");
    check(selectRenderCamera({},{},{}).source==RenderCameraSource::Fallback,"Missing camera has deterministic fallback");
    std::cout<<"Camera registration, hierarchy, selection and render ownership passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
