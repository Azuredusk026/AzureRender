#include "render/ObjectShadow.hpp"
#include "scene/TransformMath.hpp"
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace azurerender;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void near(float a,float b){require(std::abs(a-b)<2e-6F,"Object projection differs from its geometric contract");}
template<class F> void rejects(F action){bool bad=false;try{action();}catch(const std::invalid_argument&){bad=true;}require(bad,"Invalid object shadow input was accepted");}
}
int main(){try{
    ecs::ObjectShadowComponent config;config.validate();
    ObjectShadowInput input{config,scene::identityMatrix(),3};
    auto frame=buildObjectShadowFrame({input},{0,0,-1});
    require(frame.pages.size()==1&&frame.pages[0].instanceIndex==3&&frame.pages[0].page==0,"Automatic page assignment or instance ownership failed");
    const auto center=internal::transformPosition(frame.pages[0].gpu.worldToUnitShadow,{0,0,0});
    require(frame.pages[0].gpu.receiverSampling==std::array<float,4>{0,0,0,0},"Default receiver must preserve fragment position and geometry normal");
    for(float value:center)near(value,.5F);
    const auto edge=internal::transformPosition(frame.pages[0].gpu.worldToUnitShadow,{-1,1,-1});
    near(edge[0],1);near(edge[1],1);near(edge[2],1);
    const auto clip=internal::transformPosition(frame.pages[0].worldToClip,{-1,1,-1});
    near(clip[0],1);near(clip[1],1);near(clip[2],1);
    config.center={.25F,.5F,-.75F};config.halfExtents={2,3,4};config.atlasPage=6;
    config.receiverPixelOffsetX=-.5F;config.receiverPixelOffsetY=.5F;config.surfaceNormalBias=true;
    input={config,scene::composeTrs({3,4,5},{0,40,0},{-2,3,.5F}),7};
    frame=buildObjectShadowFrame({input},{.1F,-.5F,-.8F});
    require(frame.pages[0].gpu.receiverSampling==std::array<float,4>{-.5F,.5F,1,0},"Receiver sampling settings failed GPU packing");
    auto worldCenter=internal::transformPosition(input.model,config.center);
    auto projected=internal::transformPosition(frame.pages[0].gpu.worldToUnitShadow,worldCenter);
    for(float value:projected)near(value,.5F);
    near(frame.pages[0].gpu.atlasRect[0],.5F);near(frame.pages[0].gpu.atlasRect[1],.5F);
    near(frame.pages[0].gpu.atlasRect[2],.25F);near(frame.pages[0].gpu.atlasRect[3],.5F);
    auto automatic=input;automatic.instanceIndex=9;automatic.settings.atlasPage=8;
    frame=buildObjectShadowFrame({automatic,input},{0,1,0});
    require(frame.pages[0].page==0&&frame.pages[1].page==6,"Explicit pages must be reserved before automatic assignment");
    for(const auto& page:frame.pages)for(float value:page.worldToClip)require(std::isfinite(value),"Vertical light projection became nonfinite");
    auto same=input;same.instanceIndex=10;
    rejects([&]{buildObjectShadowFrame({input,same},{0,0,-1});});
    rejects([&]{buildObjectShadowFrame({automatic,automatic},{0,0,-1});});
    rejects([&]{buildObjectShadowFrame({input},{0,0,0});});
    std::vector<ObjectShadowInput> overflow(9,automatic);for(unsigned i=0;i<9;++i)overflow[i].instanceIndex=i;
    rejects([&]{buildObjectShadowFrame(overflow,{0,0,-1});});
    overflow.back().settings.enabled=false;require(buildObjectShadowFrame(overflow,{0,0,-1}).pages.size()==8,"Disabled objects must consume no page");
    for(float value:{0.F,-1.F,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){
        auto bad=config;bad.halfExtents[1]=value;rejects([&]{bad.validate();});
    }
    auto bad=config;bad.atlasPage=9;rejects([&]{bad.validate();});bad=config;bad.filterRadiusX=-1;rejects([&]{bad.validate();});
    bad=config;bad.normalBias=std::numeric_limits<float>::quiet_NaN();rejects([&]{bad.validate();});
    bad=config;bad.center[0]=std::numeric_limits<float>::infinity();rejects([&]{bad.validate();});
    for(float value:{-2.01F,2.01F,std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity()}){
        bad=config;bad.receiverPixelOffsetX=value;rejects([&]{bad.validate();});
    }
    auto overflowTransform=input;overflowTransform.model[0]=std::numeric_limits<float>::max();overflowTransform.settings.center[0]=100000;
    rejects([&]{buildObjectShadowFrame({overflowTransform},{0,0,-1});});
    std::cout<<"Object shadow projection, bounded ownership and invalid inputs passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
