#pragma once
#include "render/LinearMath.hpp"
#include "render/RenderSettings.hpp"
#include <algorithm>

namespace azurerender {
struct CameraPickRay {internal::Vector3 origin,direction;};
struct CameraScreenSegment {std::array<float,2> from,to;};
inline void validateCameraView(const CameraViewDescriptor& view,float aspect){
    for(const auto& vector:{view.position,view.target,view.up})for(float value:vector)if(!std::isfinite(value))throw std::invalid_argument("Camera pose must be finite");
    const auto forward=internal::subtract(view.target,view.position);
    if(internal::vectorLength(forward)<1e-6F||internal::vectorLength(internal::cross(forward,view.up))<1e-6F)throw std::invalid_argument("Camera requires nondegenerate forward and up vectors");
    if(!std::isfinite(aspect)||aspect<=0||!std::isfinite(view.fov)||view.fov<=.01F||view.fov>=3.13F||!std::isfinite(view.orthoHeight)||view.orthoHeight<=.00001F
        ||!std::isfinite(view.nearPlane)||!std::isfinite(view.farPlane)||view.nearPlane<=0||view.farPlane<=view.nearPlane)throw std::invalid_argument("Invalid camera projection");
}
inline internal::Matrix4 cameraViewMatrix(const CameraViewDescriptor& view){validateCameraView(view,1);return internal::lookAt(view.position,view.target,view.up);}
inline internal::Matrix4 cameraProjectionMatrix(const CameraViewDescriptor& view,float aspect){
    validateCameraView(view,aspect);
    if(view.projection==CameraProjectionKind::Orthographic){const float height=view.orthoHeight*.5F;return internal::orthographic(-height*aspect,height*aspect,-height,height,view.nearPlane,view.farPlane);}
    return internal::perspective(view.fov,aspect,view.nearPlane,view.farPlane);
}
inline CameraPickRay cameraPickRay(const CameraViewDescriptor& view,float u,float v,float aspect){
    validateCameraView(view,aspect);if(!std::isfinite(u)||!std::isfinite(v))throw std::invalid_argument("Invalid viewport coordinates");
    const auto forward=internal::normalize(internal::subtract(view.target,view.position)),right=internal::normalize(internal::cross(forward,view.up)),up=internal::cross(right,forward);
    const auto h=view.projection==CameraProjectionKind::Orthographic?view.orthoHeight*.5F:std::tan(view.fov*.5F);
    const auto offset=internal::addVectors(internal::scaleVector(right,(2*u-1)*h*aspect),internal::scaleVector(up,(1-2*v)*h));
    if(view.projection==CameraProjectionKind::Orthographic)return {internal::addVectors(view.position,offset),forward};
    return {view.position,internal::normalize(internal::addVectors(forward,offset))};
}
// Clip in Vulkan homogeneous space before dividing by w.
inline std::optional<CameraScreenSegment> projectCameraSegment(
    const CameraViewDescriptor& view,float aspect,
    const internal::Vector3& from,const internal::Vector3& to){
    const auto matrix=internal::multiply(cameraProjectionMatrix(view,aspect),cameraViewMatrix(view));
    const auto project=[&](const internal::Vector3& point){
        std::array<float,4> result{};
        for(std::size_t row=0;row<4;++row){result[row]=matrix[12+row];for(std::size_t axis=0;axis<3;++axis)result[row]+=matrix[axis*4+row]*point[axis];if(!std::isfinite(result[row]))throw std::invalid_argument("Invalid helper coordinates");}
        return result;
    };
    auto a=project(from),b=project(to);
    const auto planes=[](const std::array<float,4>& p){return std::array<float,6>{p[3]+p[0],p[3]-p[0],p[3]+p[1],p[3]-p[1],p[2],p[3]-p[2]};};
    const auto pa=planes(a),pb=planes(b);float start=0,end=1;
    for(std::size_t i=0;i<6;++i){
        if(pa[i]<0&&pb[i]<0)return std::nullopt;
        if((pa[i]<0)!=(pb[i]<0)){const auto fraction=pa[i]/(pa[i]-pb[i]);if(pa[i]<0)start=std::max(start,fraction);else end=std::min(end,fraction);}
    }
    if(start>end)return std::nullopt;
    const auto uv=[&](float t){std::array<float,4> p;for(std::size_t i=0;i<4;++i)p[i]=a[i]+(b[i]-a[i])*t;return std::array<float,2>{(p[0]/p[3]+1)*.5F,(p[1]/p[3]+1)*.5F};};
    return CameraScreenSegment{uv(start),uv(end)};
}
inline internal::Matrix4 characterProjection(const RenderSettings& settings, float aspect) {
    if(settings.cameraView)return cameraProjectionMatrix(*settings.cameraView,aspect);
    auto result=internal::perspective(3.14159265358979323846F/3,aspect,settings.cameraNear,settings.cameraFar);
    result[0]*=internal::kCharacterLensScale;result[5]*=internal::kCharacterLensScale;
    return result;
}
inline internal::Matrix4 characterView(const RenderSettings& settings,const internal::Vector3& position,const internal::Vector3& target){
    return settings.cameraView?cameraViewMatrix(*settings.cameraView):internal::lookAt(position,target,{0,1,0});
}
}
