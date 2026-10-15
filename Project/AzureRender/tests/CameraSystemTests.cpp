#include "runtime/ComponentRegistry.hpp"
#include "runtime/camera/GameCameraService.hpp"
#include "runtime/RuntimeLifecycle.hpp"
#include "ecs/CameraComponents.hpp"
#include "render/RenderCamera.hpp"
#include <iostream>
using namespace azurerender;
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(){try{
    auto registry=makeRuntimeComponentRegistry();
    const auto camera=registry.defaults("azure.camera");registry.validate("azure.camera",camera);
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
