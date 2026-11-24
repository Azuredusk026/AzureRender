#include "render/ProjectedReflectionCamera.hpp"
#include "render/CameraProjection.hpp"
#include <iostream>
#include <limits>
using namespace azurerender;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class F>void rejects(F operation){bool failed=false;try{operation();}catch(const std::exception&){failed=true;}require(failed,"Invalid projected camera accepted");}
std::array<float,4> transform(const ProjectedReflectionMatrix4& matrix,const std::array<float,4>& point){
    std::array<float,4> result{};for(unsigned row=0;row<4;++row)for(unsigned column=0;column<4;++column)result[row]+=matrix[column][row]*point[column];return result;
}
ReflectionTraceView camera(float roll=.2F){
    ReflectionTraceView value;value.projection=internal::perspective(1.1F,1.5F,.1F,100.F);
    value.projection[8]=.03F;value.projection[9]=-.05F;value.position={2,3,4};
    value.right={std::cos(roll),std::sin(roll),0};value.up={-std::sin(roll),std::cos(roll),0};value.forward={0,0,-1};
    value.worldToClip=value.projection;return value;
}
}
int main()try{
    const auto source=camera();const auto view=projectedReflectionView(source);
    require(view.convention==ProjectedReflectionConvention::VulkanUpperLeft,"Engine camera convention missing");
    ProjectedReflectionSettings settings;settings.motionEncoding=ProjectedReflectionMotionEncoding::LinearUv;
    const auto frame=view.pack({128,96},{64,48},settings);
    require(frame[29][2]==1&&frame[29][3]==1,"Motion/convention packing differs");
    for(float distance:{.1F,1.F,5.F,50.F,100.F}){
        const auto depth=(1.F/distance-view.depthReciprocal[1])/view.depthReciprocal[0];
        const auto decoded=1.F/(view.depthReciprocal[0]*depth+view.depthReciprocal[1]);
        require(std::abs(decoded-distance)<=distance*2e-6F,"Positive depth roundtrip differs");
        if(distance==.1F)require(std::abs(depth-1)<2e-5F,"Near depth must be one");
        if(distance==100.F)require(std::abs(depth)<2e-6F,"Far depth must be zero");
    }
    const std::array<float,4> relative{{source.right[0]*.3F-source.up[0]*.2F,source.right[1]*.3F-source.up[1]*.2F,-3,1}};
    auto clip=transform(view.relativeWorldToClip,relative);auto world=transform(view.clipToWorld,clip);
    require(std::abs(clip[0]/clip[3]-(source.projection[0]*.3F/3-source.projection[8]))<2e-6F,"Horizontal camera basis differs from projection");
    require(std::abs(clip[1]/clip[3]-(source.projection[5]*(-.2F)/3-source.projection[9]))<2e-6F,"Vertical camera basis differs from projection");
    for(unsigned i=0;i<3;++i)require(std::abs(world[i]/world[3]-(source.position[i]+relative[i]))<2e-4F,"Current world position roundtrip differs");
    auto previous=camera(-.15F);previous.position={3,2,4.5F};previous.projection=internal::perspective(1.3F,1.5F,.2F,70.F);previous.projection[8]=-.04F;previous.projection[9]=.02F;
    const auto history=projectedReflectionView(source,&previous);
    std::array<float,4> previousRelative{{source.position[0]+relative[0]-previous.position[0],source.position[1]+relative[1]-previous.position[1],source.position[2]+relative[2]-previous.position[2],1}};
    const internal::Vector3 offset{{previousRelative[0],previousRelative[1],previousRelative[2]}};
    const auto distance=internal::dot(previous.forward,offset);
    const std::array<float,4> previousClip{{previous.projection[0]*internal::dot(previous.right,offset)/distance-previous.projection[8],
        previous.projection[5]*internal::dot(previous.up,offset)/distance-previous.projection[9],(.2F*70/distance-.2F)/(70-.2F),1}};
    auto previousWorld=transform(history.previousClipToWorld,previousClip);
    for(unsigned i=0;i<3;++i)require(std::abs(previousWorld[i]/previousWorld[3]-(source.position[i]+relative[i]))<2e-4F,"Previous world position roundtrip differs");
    auto invalid=source;invalid.projection=internal::orthographic(-1,1,-1,1,.1F,100);rejects([&]{projectedReflectionView(invalid);});
    invalid=source;invalid.position[0]=std::numeric_limits<float>::quiet_NaN();rejects([&]{projectedReflectionView(invalid);});
    invalid=source;invalid.up={1,0,0};rejects([&]{projectedReflectionView(invalid);});
    auto bad=view;bad.convention=static_cast<ProjectedReflectionConvention>(99);rejects([&]{bad.pack({128,96},{64,48},settings);});
    auto badSettings=settings;badSettings.motionEncoding=static_cast<ProjectedReflectionMotionEncoding>(99);rejects([&]{view.pack({128,96},{64,48},badSettings);});
    std::cout<<"PASS: projected camera depth, world transforms, offsets, roll and encoding contracts\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
