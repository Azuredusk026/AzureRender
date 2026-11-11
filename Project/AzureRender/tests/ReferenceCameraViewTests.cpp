#include "render/ReferenceCameraView.hpp"
#include "render/ReferenceCameraDepth.hpp"
#include "render/ReferenceCameraViewJson.hpp"
#include "render/SceneReferenceObjectVisibility.hpp"
#include <nlohmann/json.hpp>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <vector>
using namespace azurerender;
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
bool close(float a,float b){return std::abs(a-b)<1.e-6F;}
std::vector<char> read(const char* path){std::ifstream in(path,std::ios::binary|std::ios::ate);check(bool(in),"Fixture absent");const auto n=in.tellg();check(n>0&&n<=16*1024*1024,"Fixture size invalid");std::vector<char> bytes(static_cast<std::size_t>(n));in.seekg(0);in.read(bytes.data(),n);check(bool(in),"Fixture read failed");return bytes;}
}
int main(int argc,char** argv)try{
    check(argc==1||argc==5,"Expected optional projection/frame/world/query fixtures");
    ReferenceCameraView view;view.extent={256,128};PreparedReferenceCameraView identity(view);
    const nlohmann::json legacy=view;
    check(!legacy.contains("produceSceneDepth")&&!legacy.get<ReferenceCameraView>().produceSceneDepth,
        "Legacy views keep their serialized fields and skip scene reference depth");
    auto disabledDepth=legacy;
    disabledDepth["produceSceneDepth"]=false;
    check(nlohmann::json(disabledDepth.get<ReferenceCameraView>())==legacy,
        "Explicitly disabled scene depth has the canonical legacy representation");
    auto depthDocument=legacy;
    depthDocument["produceSceneDepth"]=true;
    const auto depthView=depthDocument.get<ReferenceCameraView>();
    check(depthView.produceSceneDepth&&nlohmann::json(depthView)==depthDocument,
        "Scene reference depth must survive view persistence");
    auto visibilityDocument=depthDocument;
    visibilityDocument["produceSceneVisibilityInputs"]=true;
    const auto visibilityView=visibilityDocument.get<ReferenceCameraView>();
    check(visibilityView.produceSceneVisibilityInputs&&nlohmann::json(visibilityView)==visibilityDocument,
        "Scene visibility inputs must survive view persistence");
    auto objectDocument=visibilityDocument;
    objectDocument["objectVisibility"]={{"pixelOffset",{0.F,.5F}}};
    const auto objectView=objectDocument.get<ReferenceCameraView>();
    check(objectView.objectVisibility&&objectView.objectVisibility->pixelOffset==std::array<float,2>{0,.5F}
        &&nlohmann::json(objectView)==objectDocument,"Scene object visibility settings must survive view persistence");
    const auto rejectsObjectDocument=[](const nlohmann::json& document){
        try{(void)document.get<ReferenceCameraView>();}catch(const std::invalid_argument&){return true;}
        return false;
    };
    auto missingVisibility=objectDocument;
    missingVisibility.erase("produceSceneVisibilityInputs");
    check(rejectsObjectDocument(missingVisibility),"Scene object visibility requires scene visibility inputs");
    for(const auto& invalid:nlohmann::json::array({nullptr,0,nlohmann::json::object(),
        {{"pixelOffset",{2,0}}},{{"pixelOffset",{0}}},{{"pixelOffset",{0,0}},{"unexpected",true}}})){
        auto bad=objectDocument;
        bad["objectVisibility"]=invalid;
        check(rejectsObjectDocument(bad),"Invalid scene object visibility configuration");
    }
    auto disabledVisibility=visibilityDocument;
    disabledVisibility["produceSceneVisibilityInputs"]=false;
    check(nlohmann::json(disabledVisibility.get<ReferenceCameraView>())==depthDocument,
        "Explicitly disabled visibility inputs have the canonical depth-only representation");
    visibilityDocument.erase("produceSceneDepth");
    bool missingDepthRejected=false;
    try{(void)visibilityDocument.get<ReferenceCameraView>();}catch(const std::invalid_argument&){missingDepthRejected=true;}
    check(missingDepthRejected,"Scene visibility inputs require scene reference depth");
    for(const auto* field:{"produceSceneDepth","produceSceneVisibilityInputs"})
        for(const auto& invalid:nlohmann::json::array({0,nullptr,"true"})){
            auto rejectedDocument=depthDocument;
            rejectedDocument[field]=invalid;
            bool rejected=false;
            try{(void)rejectedDocument.get<ReferenceCameraView>();}catch(const std::invalid_argument&){rejected=true;}
            check(rejected,"Scene reference output flags require boolean values");
        }
    auto forwardDepth=depthDocument;
    forwardDepth["depthConvention"]="forward";
    bool forwardRejected=false;
    try{(void)forwardDepth.get<ReferenceCameraView>();}catch(const std::invalid_argument&){forwardRejected=true;}
    check(forwardRejected,"Scene reference depth requires reversed depth");
    const auto pixel=identity.project({.25F,-.5F,.75F});check(pixel==std::array<double,3>{160,32,.75},"Identity pixel convention");
    auto ray=identity.pickRay(.5,.5);check(ray.origin==std::array<float,3>{0,0,1}&&ray.direction==std::array<float,3>{0,0,-1},"Orthographic near ray");
    view.flipNdcY=true;PreparedReferenceCameraView flipped(view);check(flipped.project({.25F,-.5F,.75F})==std::array<double,3>{160,96,.75},"Flipped pixel convention");
    ray=flipped.pickRay(.75,.25);check(ray.origin==std::array<float,3>{.5F,.5F,1},"Flipped pick ray");
    view={};view.frame.parameters.axes={-3,1,2,0};view.frame.parameters.offset={1,.5F,0,0};PreparedReferenceCameraView permuted(view);ray=permuted.pickRay(.5,.5);check(ray.origin==std::array<float,3>{-.5F,1,1}&&ray.direction==std::array<float,3>{0,-1,0},"Frame ray basis");
    view={};view.depthConvention=ReferenceDepthConvention::Forward;PreparedReferenceCameraView forward(view);ray=forward.pickRay(.5,.5);check(ray.origin==std::array<float,3>{0,0,0}&&ray.direction==std::array<float,3>{0,0,1},"Forward near ray");
    view={};view.extent={200,100};view.projection.parameters.columns={ReferenceClipProjectionParameters::Row{2,0,0,0},{0,-4,0,0},{0,0,0,-1},{0,0,.25F,0}};PreparedReferenceCameraView perspective(view);
    check(perspective.project({.5F,.25F,-2})==std::array<double,3>{150,25,.125},"Perspective pixel oracle");ray=perspective.pickRay(.75,.25);check(ray.origin==std::array<float,3>{.0625F,.03125F,-.25F},"Perspective near origin");
    const float length=std::sqrt(1.F+.25F*.25F+.125F*.125F);check(close(ray.direction[0],.25F/length)&&close(ray.direction[1],.125F/length)&&close(ray.direction[2],-1.F/length),"Perspective ray direction");
    {
        auto calibrated=objectView;
        calibrated.frame.parameters.axes={-3,1,2,0};
        calibrated.frame.parameters.offset={1,.5F,0,0};
        calibrated.projection.parameters.columns=perspective.descriptor().projection.parameters.columns;
        calibrated.projection.parameters.jitter={.01F,-.03F,0,0};
        calibrated.flipNdcY=true;
        const PreparedReferenceCameraView camera(calibrated);
        const auto screen=sceneReferenceObjectVisibilityView(camera);
        check(screen.pixelOffset==std::array<float,2>{0,.5F}&&screen.flipNdcY
            &&screen.phaseOrigin==ScreenObjectVisibilityPhaseOrigin::TopLeft,"Reference attachment raster convention");
        const std::array<float,3> world{.25F,-2.F,1.F};
        const auto clip=camera.clip(world);
        std::array<float,4> restored{};
        for(unsigned row=0;row<4;++row)for(unsigned col=0;col<4;++col)restored[row]+=screen.clipToWorld[col*4+row]*clip[col];
        const auto expected=referenceRelativePosition(calibrated.frame,world);
        for(unsigned axis=0;axis<3;++axis)check(close(restored[axis]/restored[3],expected[axis]),"Reference inverse must use signed coordinates and offsets");
        ObjectShadowPage page;
        page.gpu.worldToUnitShadow={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
        page.gpu.biasAndFilter={.02F,.01F,4,2};
        page.gpu.atlasRect={.5F,.5F,.25F,.5F};
        const auto records=sceneReferenceObjectVisibilityRecords(calibrated.frame,{page});
        check(records.size()==1,"Current object record order");
        const auto& record=records.front();
        const auto projected=internal::transformPosition(record.worldToUnitShadow,expected);
        check(projected==world&&record.lightDirection==std::array<float,3>{-1,0,0},"Object record and light use the reference coordinate frame");
        check(record.filterRadiusUv==std::array<float,2>{1.F/1024,1.F/1024}
            &&record.lightBias==.01F&&record.normalBias==.02F&&record.atlasRect==page.gpu.atlasRect,"Object atlas record preserves authored values");
    }
    check(identity.intersectsBounds({-.5F,-.5F,.1F},{.5F,.5F,.9F}),"Inside bounds");check(!identity.intersectsBounds({2,-.5F,.1F},{3,.5F,.9F}),"Side outside bounds");check(!identity.intersectsBounds({-.5F,-.5F,1.1F},{.5F,.5F,1.5F}),"Depth outside bounds");
    check(identity.intersectsBounds({1,0,.5F},{1,0,.5F}),"Touching bounds retained");const float adjacent=std::nextafter(1.F,2.F);check(identity.intersectsBounds({adjacent,0,.5F},{adjacent,0,.5F}),"Rounding uncertainty retained");check(!identity.intersectsBounds({1.01F,0,.5F},{1.02F,0,.5F}),"Separated bounds rejected");
    {auto small=ReferenceCameraView{};for(auto& column:small.projection.parameters.columns)for(auto& value:column)value*=1.e-37F;PreparedReferenceCameraView tiny(small);check(tiny.intersectsBounds({-.5F,-.5F,-.01F},{.5F,.5F,-.01F}),"Subnormal projection flush uncertainty must retain bounds");}
    unsigned rejected=0;auto reject=[&](auto operation){try{operation();}catch(const std::invalid_argument&){++rejected;}};
    for(unsigned kind=0;kind<9;++kind)reject([&]{auto bad=ReferenceCameraView{};if(kind==0)bad.schemaVersion=0;if(kind==1)bad.reserved[1]=1;if(kind==2)bad.extent[0]=0;if(kind==3)bad.extent[1]=8193;if(kind==4)bad.depthConvention=static_cast<ReferenceDepthConvention>(99);if(kind==5)bad.frame.schemaVersion=0;if(kind==6)bad.projection.schemaVersion=0;if(kind==7)for(auto& c:bad.projection.parameters.columns)c={};if(kind==8)bad.projection.parameters.columns[0][0]=1.e-13F;(void)PreparedReferenceCameraView(bad);});
    reject([&]{(void)identity.pickRay(std::numeric_limits<double>::quiet_NaN(),.5);});reject([&]{(void)identity.unproject(.5,.5,1.1);});reject([&]{(void)identity.project({std::numeric_limits<float>::infinity(),0,0});});reject([&]{(void)identity.intersectsBounds({1,0,0},{0,1,1});});reject([&]{(void)identity.intersectsBounds({0,0,0},{std::numeric_limits<float>::infinity(),1,1});});reject([&]{(void)perspective.project({0,0,0});});reject([&]{auto bad=view;bad.projection.parameters.columns[0][0]=std::numeric_limits<float>::quiet_NaN();(void)PreparedReferenceCameraView(bad);});check(rejected==16,"Camera rejection cases");
    std::size_t nativeRays=0,visibleVertices=0;double maximumPixelError=0;
    if(argc==5){auto p=read(argv[1]),f=read(argv[2]),v=read(argv[3]),q=read(argv[4]);check(p.size()==80&&f.size()==32&&v.size()%16==0&&q.size()%16==0,"Native camera fixture layout");ReferenceCameraView native;native.extent={2560,1440};native.flipNdcY=true;std::memcpy(&native.projection.parameters,p.data(),80);std::memcpy(&native.frame.parameters,f.data(),32);PreparedReferenceCameraView camera(native);const auto config=referenceCameraDepthConfig(camera);check(config.schemaVersion==2&&config.flipNdcY&&config.positionFrame&&config.convention==native.depthConvention,"Prepared camera depth adaptation");
        const auto vertices=reinterpret_cast<const std::array<float,4>*>(v.data());for(std::size_t i=0;i<v.size()/16;++i){std::array<float,3> point{vertices[i][0],vertices[i][1],vertices[i][2]};const auto clip=camera.clip(point);if(clip[3]>=std::abs(clip[0])&&clip[3]>=std::abs(clip[1])&&clip[2]>=0&&clip[2]<=clip[3]){check(camera.intersectsBounds(point,point),"Visible native vertex culled");++visibleVertices;}}
        const auto queries=reinterpret_cast<const std::array<float,4>*>(q.data());for(std::size_t i=0;i<q.size()/16;++i){const double x=queries[i][0]+.5,y=queries[i][1]+.5;const auto ray=camera.pickRay(x/2560,y/1440);std::array<float,3> point;for(unsigned axis=0;axis<3;++axis)point[axis]=ray.origin[axis]+ray.direction[axis];const auto pixel=camera.project(point);maximumPixelError=std::max(maximumPixelError,std::max(std::abs(pixel[0]-x),std::abs(pixel[1]-y)));++nativeRays;}
        check(nativeRays==17017&&maximumPixelError<=1./256,"Native ray projection exceeds one eight-bit subpixel");
    }
    std::cout<<"Reference camera: independent perspective/orthographic/axis/flip and conservative bounds passed; "<<rejected<<" rejects; nativeRays="<<nativeRays<<" visibleVertices="<<visibleVertices<<" maximumPixelError="<<maximumPixelError<<"\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
