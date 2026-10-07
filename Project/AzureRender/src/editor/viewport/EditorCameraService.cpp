#include "EditorCameraService.hpp"
#include "render/RenderMath.hpp"
#include <cmath>
namespace azurerender {
FrameSelectionResult EditorCameraService::frameSelection(const SelectionBoundsResult& result,std::array<float,3> position,std::array<float,3> target,float fov,float aspect,float nearPlane,float farPlane){
    if(!result.valid)return {false,{}, {},0,result.diagnostic};
    if(!std::isfinite(fov)||fov<=.01F||fov>=3.13F||!std::isfinite(aspect)||aspect<=0||nearPlane<=0||farPlane<=nearPlane)
        return {false,{}, {},0,"Invalid camera projection"};
    using namespace internal;
    for(std::size_t axis=0;axis<3;++axis)if(!std::isfinite(position[axis])||!std::isfinite(target[axis]))return {false,{}, {},0,"Invalid camera pose"};
    const auto difference=subtract(target,position);
    auto forward=dot(difference,difference)>1e-10F?normalize(difference):Vector3{0,0,-1};
    const auto lateral=cross(forward,{0,1,0});
    const auto right=dot(lateral,lateral)>1e-10F?normalize(lateral):Vector3{1,0,0};
    const auto up=normalize(cross(right,forward));
    Vector3 centre;
    for(std::size_t axis=0;axis<3;++axis)centre[axis]=(result.bounds.minimum[axis]+result.bounds.maximum[axis])*.5F;
    float distance=nearPlane;
    for(unsigned corner=0;corner<8;++corner){
        Vector3 offset;
        for(unsigned axis=0;axis<3;++axis)offset[axis]=((corner&(1U<<axis))?result.bounds.maximum[axis]:result.bounds.minimum[axis])-centre[axis];
        const auto depth=dot(offset,forward);
        distance=std::max(distance,std::max(std::abs(dot(offset,right))/(std::tan(fov*.5F)*aspect*.9F),std::abs(dot(offset,up))/(std::tan(fov*.5F)*.9F))-depth);
        distance=std::max(distance,nearPlane*1.1F-depth);
    }
    for(unsigned corner=0;corner<8;++corner){
        Vector3 offset;
        for(unsigned axis=0;axis<3;++axis)offset[axis]=((corner&(1U<<axis))?result.bounds.maximum[axis]:result.bounds.minimum[axis])-centre[axis];
        if(distance+dot(offset,forward)>=farPlane)return {false,{}, {},0,"Selection exceeds camera clipping range"};
    }
    for(std::size_t axis=0;axis<3;++axis)position[axis]=centre[axis]-forward[axis]*distance;
    return {true,position,centre,distance,{}};
}
}
