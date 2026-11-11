#pragma once

#include "render/ObjectShadowAtlas.hpp"
#include "render/ReferenceCameraView.hpp"
#include "render/ScreenObjectVisibility.hpp"

namespace azurerender {

inline ScreenObjectVisibilityView sceneReferenceObjectVisibilityView(const PreparedReferenceCameraView& camera){
    ScreenObjectVisibilityView result;
    result.clipToWorld=camera.clipToReference();
    result.pixelOffset=camera.descriptor().objectVisibility.value().pixelOffset;
    // The reference pass reverses Y relative to the prepared camera projection.
    result.flipNdcY=true;
    result.phaseOrigin=ScreenObjectVisibilityPhaseOrigin::TopLeft;
    return result;
}

inline std::vector<ScreenObjectVisibilityRecord> sceneReferenceObjectVisibilityRecords(const ReferencePositionFrame& frame,
    const std::vector<ObjectShadowPage>& pages){
    validateReferencePositionFrame(frame);
    std::vector<ScreenObjectVisibilityRecord> result;
    result.reserve(pages.size());
    for(const auto& page:pages){
        const auto& object=page.gpu;
        const auto& worldProjection=object.worldToUnitShadow;
        const auto direction=internal::normalize({worldProjection[2],worldProjection[6],worldProjection[10]});
        ScreenObjectVisibilityRecord record;
        std::array<double,4> translation{};
        for(unsigned row=0;row<4;++row)translation[row]=worldProjection[12+row];
        for(unsigned axis=0;axis<3;++axis){
            const int choice=frame.parameters.axes[axis];
            const auto source=static_cast<unsigned>(std::abs(choice)-1);
            const float sign=choice<0?-1.F:1.F;
            record.lightDirection[axis]=sign*direction[source];
            for(unsigned row=0;row<4;++row){
                const float value=sign*worldProjection[source*4+row];
                record.worldToUnitShadow[axis*4+row]=value;
                translation[row]-=double(value)*frame.parameters.offset[axis];
            }
        }
        for(unsigned row=0;row<4;++row)record.worldToUnitShadow[12+row]=static_cast<float>(translation[row]);
        record.normalBias=object.biasAndFilter[0];
        record.lightBias=object.biasAndFilter[1];
        record.filterRadiusUv={object.biasAndFilter[2]/ObjectShadowAtlas::kWidth,
            object.biasAndFilter[3]/ObjectShadowAtlas::kHeight};
        record.atlasRect=object.atlasRect;
        result.push_back(record);
    }
    return result;
}

}
