#include "EditorCameraService.hpp"
#include "render/RenderMath.hpp"
#include "render/CameraProjection.hpp"
#include <cmath>
#include <map>
namespace azurerender {
void EditorCameraService::projection(EditorViewDescriptor& view,CameraProjectionKind kind){
    validateCameraView(view,1);
    if(view.projection!=kind && kind==CameraProjectionKind::Orthographic)view.orthoHeight=2*internal::vectorLength(internal::subtract(view.target,view.position))*std::tan(view.fov*.5F);
    view.projection=kind;
}
void EditorCameraService::orient(EditorViewDescriptor& view,const std::string& axis){
    const std::map<std::string,internal::Vector3> directions={{"+X",{1,0,0}},{"-X",{-1,0,0}},{"+Y",{0,1,0}},{"-Y",{0,-1,0}},{"+Z",{0,0,1}},{"-Z",{0,0,-1}}};
    const auto found=directions.find(axis);if(found==directions.end())throw std::invalid_argument("Unknown viewport orientation");
    projection(view,CameraProjectionKind::Orthographic);
    const auto distance=std::max(.001F,internal::vectorLength(internal::subtract(view.position,view.target)));
    view.position=internal::addVectors(view.target,internal::scaleVector(found->second,distance));view.up=axis=="+Y"?internal::Vector3{0,0,-1}:axis=="-Y"?internal::Vector3{0,0,1}:internal::Vector3{0,1,0};
    validateCameraView(view,1);
}
FrameSelectionResult EditorCameraService::frameSelection(EditorViewDescriptor& view,const SelectionBoundsResult& bounds,float aspect){
    if(!bounds.valid)return {false,{}, {},0,bounds.diagnostic};
    try{validateCameraView(view,aspect);}catch(const std::exception& error){return {false,{}, {},0,error.what()};}
    const auto forward=internal::normalize(internal::subtract(view.target,view.position)),right=internal::normalize(internal::cross(forward,view.up)),up=internal::cross(right,forward);
    auto candidate=view;for(unsigned a=0;a<3;++a)candidate.target[a]=(bounds.bounds.minimum[a]+bounds.bounds.maximum[a])*.5F;
    float distance=view.nearPlane,height=.001F;
    for(unsigned corner=0;corner<8;++corner){internal::Vector3 offset;for(unsigned a=0;a<3;++a)offset[a]=((corner&(1U<<a))?bounds.bounds.maximum[a]:bounds.bounds.minimum[a])-candidate.target[a];
        const auto depth=internal::dot(offset,forward);const float tangent=std::tan(view.fov*.5F);
        const float x=internal::dot(offset,right)/aspect+(view.projection==CameraProjectionKind::Perspective?view.projectionOffsetX*depth*tangent:0);
        const float y=internal::dot(offset,up)-(view.projection==CameraProjectionKind::Perspective?view.projectionOffsetY*depth*tangent:0);
        const float extent=std::max(std::abs(x),std::abs(y))/.9F;
        height=std::max(height,extent*2);distance=std::max(distance,std::max(view.nearPlane*1.1F,view.projection==CameraProjectionKind::Perspective?extent/std::tan(view.fov*.5F):view.nearPlane*1.1F)-depth);
    }
    for(unsigned corner=0;corner<8;++corner){internal::Vector3 offset;for(unsigned a=0;a<3;++a)offset[a]=((corner&(1U<<a))?bounds.bounds.maximum[a]:bounds.bounds.minimum[a])-candidate.target[a];if(distance+internal::dot(offset,forward)>=view.farPlane)return {false,{}, {},0,"Selection exceeds camera clipping range"};}
    candidate.position=internal::subtract(candidate.target,internal::scaleVector(forward,distance));if(view.projection==CameraProjectionKind::Orthographic)candidate.orthoHeight=height;
    const float halfHeight=view.projection==CameraProjectionKind::Orthographic?height*.5F:distance*std::tan(view.fov*.5F);
    const auto shift=cameraProjectionRayOffset(view,right,up,halfHeight*aspect,halfHeight);
    candidate.position=internal::subtract(candidate.position,shift);candidate.target=internal::subtract(candidate.target,shift);view=candidate;
    return {true,view.position,view.target,distance,{}};
}
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
