#include "render/ReferenceCameraCalibration.hpp"
#include "ecs/ReferenceCameraComponents.hpp"
#include "runtime/camera/GameCameraService.hpp"
#include "runtime/GameRuntime.hpp"
#include "runtime/RuntimeLifecycle.hpp"
#include "runtime/ComponentRegistry.hpp"
#include "runtime/ComponentCodec.hpp"
#include "render/ReferenceCameraViewJson.hpp"
#include "scene/TransformMath.hpp"
#include "scene/SceneDescription.hpp"
#include <iostream>
#include <fstream>
#include <cstring>
using namespace azurerender;
namespace {
void check(bool v,const char* m){if(!v)throw std::runtime_error(m);}
template<class F> void rejects(F f){bool caught=false;try{f();}catch(const std::exception&){caught=true;}check(caught,"Expected reference camera refusal");}
}
int main(int argc,char** argv)try{
    ReferenceCameraView source;source.extent={2560,1440};source.projection.parameters.columns={ReferenceClipProjectionParameters::Row{2,0,0,0},{0,3,0,0},{0,0,0,-1},{0,0,.1F,0}};
    if(argc!=1&&argc!=3)throw std::invalid_argument("Expected optional source projection and frame");
    if(argc==3){std::ifstream p(argv[1],std::ios::binary),f(argv[2],std::ios::binary);p.read(reinterpret_cast<char*>(&source.projection.parameters),80);f.read(reinterpret_cast<char*>(&source.frame.parameters),32);check(p.gcount()==80&&f.gcount()==32,"Invalid source profile fixture");}
    ReferenceCameraPose anchor;const auto unchanged=relocateReferenceCamera(source,anchor,anchor);
    check(std::memcmp(&source.projection.parameters,&unchanged.projection.parameters,80)==0&&std::memcmp(&source.frame.parameters,&unchanged.frame.parameters,32)==0,"Anchor must preserve original parameter bits");
    auto invalid=anchor;invalid.up=invalid.right;rejects([&]{(void)relocateReferenceCamera(source,invalid,anchor);});invalid=anchor;invalid.back[2]=-1;rejects([&]{(void)relocateReferenceCamera(source,anchor,invalid);});
    auto registry=makeRuntimeComponentRegistry();auto defaults=registry.defaults("azure.reference-camera");registry.validate("azure.reference-camera",defaults);
    rejects([&]{registry.validateComposition(nlohmann::json{{"azure.reference-camera",defaults}});});
    auto bad=defaults;bad["data"]["enabled"]=true;rejects([&]{registry.validate("azure.reference-camera",bad);});
    bad=defaults;bad["data"]["anchorUp"]={1,0,0};rejects([&]{registry.validate("azure.reference-camera",bad);});
    bad=defaults;bad["version"]=2;rejects([&]{registry.validate("azure.reference-camera",bad);});
    bad=defaults;bad["data"]["view"]=std::string(65,'a');rejects([&]{registry.validate("azure.reference-camera",bad);});
    SceneDocument document;SceneNode camera;camera.id="camera";document.nodes.push_back(camera);SceneNode other;other.id="camera-z";document.nodes.push_back(other);RuntimeLifecycle runtime;runtime.loadScene(document);
    runtime.world().addComponent(runtime.entity("camera"),game::GameCamera{});game::ReferenceCamera binding;binding.enabled=true;binding.view="calibrated";runtime.world().addComponent(runtime.entity("camera"),binding);
    ReferenceCameraViews views{{"calibrated",source}};auto selected=resolveGameCamera(runtime,document.renderDescription(),views);
    check(selected.view&&selected.referenceView&&selected.referenceName=="calibrated","Named reference camera was not selected");
    check(nlohmann::json(*selected.referenceView)==nlohmann::json(source),"Selected anchor profile changed");
    auto encoded=registry.encode("azure.reference-camera",runtime.world(),runtime.entity("camera"));game::ReferenceCamera restored;registry.metadata().decode("azure.reference-camera",&restored,nlohmann::json::parse(encoded.dump()));
    check(restored.view==binding.view&&restored.anchorRight==binding.anchorRight&&restored.anchorPosition==binding.anchorPosition,"Camera binding round trip failed");
    document.nodes[0].translation={1,2,3};document.nodes[0].rotation={0,90,0};const auto world=scene::composeTrs(document.nodes[0].translation,document.nodes[0].rotation,{1,1,1});
    selected=resolveGameCamera(runtime,document.renderDescription(),views);check(selected.referenceView.has_value(),"Moving reference camera missing");
    const PreparedReferenceCameraView before(source),after(*selected.referenceView);double maximum=0;
    for(unsigned i=0;i<256;++i){const std::array<float,3> p{float(int(i%17)-8)/16,float(i%23)/16,-2-float(i%7)};std::array<float,3> moved{};
        for(unsigned row=0;row<3;++row){moved[row]=world[12+row];for(unsigned c=0;c<3;++c)moved[row]+=world[c*4+row]*p[c];}
        const auto a=before.project(p),b=after.project(moved);for(unsigned axis=0;axis<2;++axis)maximum=std::max(maximum,std::abs(a[axis]-b[axis]));}
    check(maximum<=1./256,"Rigid camera motion exceeds pixel round trip budget");
    ReferenceCameraView ortho;ortho.extent={640,480};const auto pose=referenceCameraPose(selected.view->position,selected.view->target,selected.view->up);const PreparedReferenceCameraView movedOrtho(relocateReferenceCamera(ortho,anchor,pose));
    for(unsigned i=0;i<16;++i){const std::array<float,3> p{float(i)/16,.25F,-2};std::array<float,3> q{};for(unsigned row=0;row<3;++row){q[row]=world[12+row];for(unsigned c=0;c<3;++c)q[row]+=world[c*4+row]*p[c];}const auto a=PreparedReferenceCameraView(ortho).project(p),b=movedOrtho.project(q);check(std::abs(a[0]-b[0])<1./256&&std::abs(a[1]-b[1])<1./256,"Orthographic calibration motion failed");}
    auto missing=resolveGameCamera(runtime,document.renderDescription(),{});check(!missing.view&&missing.referenceRejected&&!missing.diagnostic.empty(),"Missing reference must be observable");
    GameRuntime game(runtime);game.setReferenceViews(views);check(game.renderCamera(document).referenceView.has_value(),"Game runtime must consume installed view collection");
    auto invalidViews=views;invalidViews.begin()->second.frame.schemaVersion=2;rejects([&]{game.setReferenceViews(invalidViews);});check(game.renderCamera(document).referenceView.has_value(),"Failed collection update must retain previous snapshot");
    game.setReferenceViews({});game.activateCamera(game::ThirdPersonCamera{},{0,0,0});check(!game.renderCamera(document).view,"Required reference failure must retain its diagnostic rather than use follow fallback");
    runtime.world().tryGet<game::ReferenceCamera>(runtime.entity("camera"))->enabled=false;check(game.renderCamera(document).view&&!game.renderCamera(document).referenceView,"Disabled binding must retain ordinary game camera");
    game.setReferenceViews(views);runtime.world().tryGet<game::ReferenceCamera>(runtime.entity("camera"))->enabled=true;
    game::GameCamera alternate;alternate.priority=1;runtime.world().addComponent(runtime.entity("camera-z"),alternate);
    const auto higher=game.renderCamera(document);check(higher.node=="camera-z"&&!higher.referenceView&&higher.referenceName.empty(),"Higher priority ordinary camera must clear reference selection metadata");
    runtime.world().tryGet<game::GameCamera>(runtime.entity("camera-z"))->priority=0;const auto tied=game.renderCamera(document);check(tied.node=="camera"&&tied.referenceView&&tied.referenceName=="calibrated","Mixed camera priority ties must retain stable node ordering");
    check(views.at("calibrated").frame.parameters.offset==source.frame.parameters.offset,"Selection must own values and preserve source collection");
    std::cout<<"Reference camera registration, composition, anchor bits, 256 rigid-motion points, missing/disabled/follow contracts, persistence and owned values passed; maximumPixelError="<<maximum<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
