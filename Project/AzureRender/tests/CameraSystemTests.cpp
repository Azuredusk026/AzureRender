#include "runtime/ComponentRegistry.hpp"
#include "runtime/camera/GameCameraService.hpp"
#include "runtime/RuntimeLifecycle.hpp"
#include "runtime/GameRuntime.hpp"
#include "ecs/CameraComponents.hpp"
#include "render/RenderCamera.hpp"
#include "scene/TransformMath.hpp"
#include <iostream>
#include "scene/SceneDescription.hpp"
using namespace azurerender;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void checkSharedRenderScene(){
    SceneDocument document;document.nodes={{"parent","Parent"},{"camera","Camera","parent"}};
    document.nodes[0].translation={10,0,0};document.nodes[1].translation={0,2,3};
    RuntimeLifecycle runtime;runtime.loadScene(document);runtime.start();
    runtime.world().addComponent(runtime.entity("camera"),game::GameCamera{});
    auto* camera=runtime.world().tryGet<game::GameCamera>(runtime.entity("camera"));
    GameRuntime game(runtime);game.setBeforeStep([&](double){
        runtime.world().tryGet<ecs::TransformComponent>(runtime.entity("parent"))->translation[0]=14;
        runtime.world().tryGet<ecs::TransformComponent>(runtime.entity("camera"))->translation[2]=6;
    });
    game.advance(1.0/60);game.advance(1.0/120);
    const auto snapshot=game.renderScene();const auto selected=game.renderCamera(snapshot);
    check(selected.node=="camera"&&selected.view.has_value(),"Rendered scene camera must preserve authored selection");
    const auto prepared=snapshot.renderDescription();
    const auto preparedSelection=game.renderCamera(prepared);
    check(preparedSelection.node==selected.node&&preparedSelection.view->position==selected.view->position
        &&preparedSelection.view->target==selected.view->target&&preparedSelection.view->up==selected.view->up
        &&preparedSelection.view->fov==selected.view->fov,"Prepared render description must preserve camera selection and lens");
    check(resolveGameCamera(runtime,prepared).node==selected.node,"Camera service must accept the shared render description");
    check(std::abs(snapshot.nodes[0].translation[0]-12)<1e-5F,"Frame scene must retain the interpolation phase");
    check(selected.view->position==std::array<float,3>{12,2,4.5F},"Camera and geometry must share interpolated parent and local transforms");
    runtime.world().tryGet<ecs::TransformComponent>(runtime.entity("parent"))->translation[0]=40;
    check(game.renderCamera(snapshot).view->position==selected.view->position,"Camera selection must use the caller's scene instead of fetching a newer scene");
    check(game.renderCamera(prepared).view->position==selected.view->position,"Prepared camera input must remain independent of newer world transforms");
    const auto live=game.renderScene();check(game.renderCamera().view->position==game.renderCamera(live).view->position,"Standalone camera calls must preserve their existing scene acquisition behavior");
    camera->enabled=false;game.activateCamera(game::ThirdPersonCamera{},std::array<float,3>{1,2,3});
    const auto followed=game.renderCamera(snapshot);
    const auto preparedFollow=game.renderCamera(prepared);
    check(preparedFollow.node==followed.node&&preparedFollow.view->position==followed.view->position,
        "Prepared render input must retain the live follow-camera fallback");
    check(followed.node=="follow-camera"&&followed.view->position==game.renderCameraPosition(),"Missing authored cameras must preserve interpolated follow-camera selection");
    game.deactivateCamera();check(!game.renderCamera(prepared).view,"Prepared input retains missing-camera fallback");check(!game.renderCamera(snapshot).view,"Missing authored and follow cameras must remain observable");
    check(snapshot.nodes[0].translation[0]==12&&selected.view->position[0]==12,"Returned camera and frame scene must retain their owned values");
}
int main(){try{
    checkSharedRenderScene();
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
    check(resolveGameCamera(runtime,scene.renderDescription()).node=="a","Prepared render description retains stable priority ties");
    runtime.world().tryGet<game::GameCamera>(runtime.entity("a"))->enabled=false;
    check(resolveGameCamera(runtime,scene).node=="b","Disabled cameras are excluded");
    check(resolveGameCamera(runtime,scene.renderDescription()).node=="b","Prepared render description retains live disabled-camera exclusion");
    CameraViewDescriptor editing;editing.position={50,50,50};editing.target={0,0,0};
    const auto finalEdit=selectRenderCamera(editing,selected.view,{});
    check(finalEdit.source==RenderCameraSource::SceneView&&finalEdit.view.position==editing.position,"Simulation preserves SceneViewCamera");
    check(selectRenderCamera({},selected.view,{}).source==RenderCameraSource::GameCamera,"Play selects GameCamera");
    check(selectRenderCamera({},{},{}).source==RenderCameraSource::Fallback,"Missing camera has deterministic fallback");
    std::cout<<"Camera registration, hierarchy, selection and render ownership passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
