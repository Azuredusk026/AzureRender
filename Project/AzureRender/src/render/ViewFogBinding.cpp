#include "render/ViewFogBinding.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace azurerender {
namespace {
using Vector=std::array<double,3>;
Vector unit(Vector value){const double n=std::hypot(value[0],value[1],value[2]);if(n<1e-8)throw std::invalid_argument("Fog capture view has a degenerate basis");for(double& x:value)x/=n;return value;}
Vector cross(const Vector& a,const Vector& b){return {a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};}
std::array<Vector,2> basis(const CameraViewDescriptor& view){
    for(const auto& vector:{view.position,view.target,view.up})for(float value:vector)if(!std::isfinite(value))throw std::invalid_argument("Fog camera requires finite coordinates");
    for(float value:{view.fov,view.orthoHeight,view.nearPlane,view.farPlane,view.projectionOffsetX,view.projectionOffsetY})if(!std::isfinite(value))throw std::invalid_argument("Fog camera requires finite lens values");
    if((view.projection!=CameraProjectionKind::Perspective&&view.projection!=CameraProjectionKind::Orthographic)||view.fov<=0||view.fov>=3.14159265358979323846F
        ||view.orthoHeight<=0||view.nearPlane<=0||view.farPlane<=view.nearPlane)throw std::invalid_argument("Fog capture lens is outside its domain");
    const auto forward=unit({double(view.target[0])-view.position[0],double(view.target[1])-view.position[1],double(view.target[2])-view.position[2]});
    const auto right=unit(cross(forward,{view.up[0],view.up[1],view.up[2]}));return {forward,cross(right,forward)};
}
}
void ViewFogBinding::validate() const {
    projection.validate();if(enabled&&(asset.empty()||!captureView))throw std::invalid_argument("Enabled view fog requires a volume and captured view");
    if(captureView)(void)basis(*captureView);
}
FogViewMatch ViewFogBinding::match(const CameraViewDescriptor& current,float aspect) const {
    validate();if(!enabled)return FogViewMatch::Disabled;
    if(!std::isfinite(aspect)||aspect<=0)throw std::invalid_argument("Fog view aspect must be finite and positive");
    const auto a=basis(*captureView),b=basis(current);const auto& reference=*captureView;
    if(current.projection!=reference.projection)return FogViewMatch::CameraMismatch;
    for(unsigned i=0;i<3;++i)if(std::abs(double(current.position[i])-reference.position[i])>1e-4||std::abs(a[0][i]-b[0][i])>1e-5||std::abs(a[1][i]-b[1][i])>1e-5)return FogViewMatch::CameraMismatch;
    const float lens=current.projection==CameraProjectionKind::Perspective?current.fov:current.orthoHeight;
    const float capturedLens=current.projection==CameraProjectionKind::Perspective?reference.fov:reference.orthoHeight;
    if(std::abs(lens-capturedLens)>1e-5F||std::abs(current.projectionOffsetX-reference.projectionOffsetX)>1e-5F||std::abs(current.projectionOffsetY-reference.projectionOffsetY)>1e-5F)return FogViewMatch::CameraMismatch;
    const double expected=double(projection.extent[0])/projection.extent[1];
    return std::abs(aspect-expected)>1e-5*std::max(1.,expected)?FogViewMatch::AspectMismatch:FogViewMatch::Matched;
}
std::string_view fogViewMatchName(FogViewMatch state) noexcept {
    switch(state){case FogViewMatch::Disabled:return "disabled";case FogViewMatch::Matched:return "matched";case FogViewMatch::CameraMismatch:return "camera-mismatch";case FogViewMatch::AspectMismatch:return "aspect-mismatch";}
    return "unknown";
}
}
