#pragma once
#include "render/ReferenceCameraView.hpp"
#include "render/ReferenceDepth.hpp"
namespace azurerender {
inline ReferenceDepthConfig referenceCameraDepthConfig(const PreparedReferenceCameraView& camera){
    const auto& view=camera.descriptor();ReferenceDepthConfig result;result.schemaVersion=2;
    result.positionFrame=view.frame;result.convention=view.depthConvention;result.flipNdcY=view.flipNdcY;return result;
}
}
