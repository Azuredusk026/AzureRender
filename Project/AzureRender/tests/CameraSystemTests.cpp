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
GameCameraSelection fullTraversalCamera(RuntimeLifecycle& runtime,const scene::SceneDescription& scene){
    GameCameraSelection result;std::int64_t priority=-1;
    const auto matrices=scene::resolveNodeWorldTransforms(scene);
    for(std::size_t i=0;i<scene.nodes.size();++i){
        const auto& node=scene.nodes[i];
        const auto* camera=runtime.world().tryGet<game::GameCamera>(runtime.entity(node.id));
        if(!camera||!camera->enabled||camera->priority<priority
            ||(camera->priority==priority&&!result.node.empty()&&node.id>=result.node))continue;
        CameraViewDescriptor view;
        try{view=gameCameraView(*camera,matrices[i]);}catch(const std::exception& error){result.diagnostic=node.id+": "+error.what();continue;}
        result.view=view;result.node=node.id;priority=camera->priority;
    }
    if(!result.view&&result.diagnostic.empty())result.diagnostic="No enabled GameCamera";
    return result;
}
void checkCameraTraversal(){
    SceneDocument document;
    for(unsigned i=0;i<256;++i){
        SceneNode node;node.id="node-"+std::to_string(i);
        if(i)node.parentId="node-"+std::to_string((i-1)/3);
        node.translation={float(i)*.125F,-float(i%7),float(i%11)};
        node.rotation={float(i%17),float(i%31),-float(i%13)};
        node.scale={1.F+float(i%3)*.1F,1,1};document.nodes.push_back(node);
    }
    RuntimeLifecycle runtime;runtime.loadScene(document);
    const auto verify=[&](const scene::SceneDescription& description){
        const auto a=resolveGameCamera(runtime,description);const auto b=fullTraversalCamera(runtime,description);
        check(a.node==b.node&&a.diagnostic==b.diagnostic&&a.view.has_value()==b.view.has_value(),
            "Sparse camera traversal must preserve selection and diagnostics");
        if(a.view){const auto& x=*a.view;const auto& y=*b.view;
            check(x.position==y.position&&x.target==y.target&&x.up==y.up&&x.projection==y.projection
                &&x.fov==y.fov&&x.orthoHeight==y.orthoHeight&&x.nearPlane==y.nearPlane&&x.farPlane==y.farPlane
                &&x.projectionOffsetX==y.projectionOffsetX&&x.projectionOffsetY==y.projectionOffsetY,
                "Sparse camera transforms and lenses must exactly match complete traversal");}
    };
    auto description=document.renderDescription();verify(description);
    for(unsigned i:{13U,127U,255U})runtime.world().addComponent(runtime.entity("node-"+std::to_string(i)),game::GameCamera{});
    for(unsigned pass=0;pass<24;++pass){
        for(unsigned i:{13U,127U,255U}){
            auto& camera=*runtime.world().tryGet<game::GameCamera>(runtime.entity("node-"+std::to_string(i)));
            camera.priority=(pass+i)%4;camera.enabled=(pass+i)%5!=0;
            camera.orthographic=pass%2!=0;camera.projectionOffsetX=float(pass%3)*.125F;
            camera.nearPlane=(pass+i)%7==0?0.F:.1F;
        }
        verify(description);std::reverse(description.nodes.begin(),description.nodes.end());verify(description);
    }
    auto* camera=runtime.world().tryGet<game::GameCamera>(runtime.entity("node-13"));
    camera->enabled=true;camera->nearPlane=.1F;camera->priority=100;
    description=document.renderDescription();description.nodes[1].parentId="node-13";
    verify(description);std::reverse(description.nodes.begin(),description.nodes.end());verify(description);
    description=document.renderDescription();description.nodes.push_back(description.nodes[13]);
    description.nodes.back().parentId.clear();description.nodes.back().translation={99,88,77};
    description.nodes[13].scale={0,0,0};verify(description);
    runtime.world().destroyEntity(runtime.entity("node-13"));
    const auto recycled=runtime.world().createEntity();runtime.world().addComponent(recycled,game::GameCamera{});
    verify(description);
    for(unsigned i:{127U,255U})runtime.world().removeComponent<game::GameCamera>(runtime.entity("node-"+std::to_string(i)));
    verify(description);
}
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
    SceneDocument reusable;game.renderScene(reusable);
    const auto* retained=reusable.nodes.data();game.renderScene(reusable);
    check(reusable.nodes.data()==retained&&reusable.nodes[0].translation==snapshot.nodes[0].translation
        &&reusable.nodes[1].translation==snapshot.nodes[1].translation,
        "Owned render-scene reuse must preserve storage and interpolation arithmetic");
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
    checkCameraTraversal();
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
