#pragma once
#include "render/ReferenceCameraView.hpp"
namespace azurerender {
// Immutable, borrowed-value upload data. No GPU objects or retained references.
struct alignas(16) ReferenceRasterProjectionParameters {
    ReferenceClipProjectionParameters projection;
    // Version, packed frame axes, flags (enabled=1, flipY=2, outline depth=4), reserved.
    std::array<std::uint32_t,4> control{1,136,1,0};
    std::array<float,4> offset{};
};
static_assert(sizeof(ReferenceRasterProjectionParameters)==112);
static_assert(std::is_standard_layout_v<ReferenceRasterProjectionParameters>);
inline ReferenceRasterProjectionParameters referenceRasterProjectionParameters(const PreparedReferenceCameraView& camera,bool enabled=true){
    const auto& view=camera.descriptor();ReferenceRasterProjectionParameters p;
    p.projection=view.projection.parameters;
    p.control={1,referencePositionFrameCode(view.frame),(enabled?1U:0U)|(view.flipNdcY?2U:0U),0};
    p.offset=view.frame.parameters.offset;
    if(enabled&&view.outlineDepthProjection.schemaVersion==1){
        p.control[2]|=4U;
        p.projection.jitter[2]=view.outlineDepthProjection.clipWSlope;
        p.projection.jitter[3]=view.outlineDepthProjection.clipZIntercept;
    }
    return p;
}
inline std::array<float,4> composeReferenceRasterPosition(const std::array<float,4>& referenceClip,const std::array<float,4>& deviceBase,const std::array<float,4>& deviceFinal,bool enabled=true){
    for(const auto& vector:{referenceClip,deviceBase,deviceFinal})for(float v:vector)if(!std::isfinite(v))throw std::invalid_argument("Raster composition inputs must be finite");
    if(!enabled||referenceClip[3]==0||deviceBase[3]==0||deviceFinal[3]==0)return deviceFinal;
    auto result=referenceClip;
    // Retain the device depth convention and clip-space depth effects.
    volatile float depth=deviceFinal[2]/deviceFinal[3];result[2]=depth*result[3];
    if(deviceBase[0]!=deviceFinal[0]||deviceBase[1]!=deviceFinal[1]||deviceBase[3]!=deviceFinal[3]){
        for(unsigned axis=0;axis<2;++axis){volatile float finalNdc=deviceFinal[axis]/deviceFinal[3],baseNdc=deviceBase[axis]/deviceBase[3];
            volatile float delta=finalNdc-baseNdc,offset=delta*result[3];result[axis]=result[axis]+offset;}
    }
    for(float v:result)if(!std::isfinite(v))return deviceFinal;
    return result;
}
inline std::array<float,4> projectReferenceRasterPosition(const PreparedReferenceCameraView& camera,const std::array<float,3>& world,const std::array<float,4>& deviceBase,const std::array<float,4>& deviceFinal,bool enabled=true){
    if(!enabled)return composeReferenceRasterPosition({0,0,0,1},deviceBase,deviceFinal,false);
    return composeReferenceRasterPosition(camera.clip(world),deviceBase,deviceFinal);
}
}
