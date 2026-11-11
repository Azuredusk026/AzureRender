#pragma once

#include "render/ObjectShadowAtlas.hpp"
#include "render/ReferenceRasterProjection.hpp"
#include "render/ScreenObjectVisibility.hpp"
#include "scene/TransformEditing.hpp"

namespace azurerender {

struct alignas(16) SceneReferenceObjectShadowRasterParameters {
    ReferenceRasterProjectionParameters raster;
    std::array<float,4> absoluteOrigin{};
};
static_assert(sizeof(SceneReferenceObjectShadowRasterParameters)==128);
static_assert(offsetof(SceneReferenceObjectShadowRasterParameters,absoluteOrigin)==112);
using SceneReferenceObjectShadowRasterArray=std::array<SceneReferenceObjectShadowRasterParameters,kObjectShadowCapacity>;

inline ReferencePositionFrame sceneReferenceObjectVisibilityFrame(const PreparedReferenceCameraView& camera){
    auto frame=camera.descriptor().frame;
    const auto& calibration=camera.descriptor().objectVisibility.value().calibration;
    if(calibration)for(unsigned axis=0;axis<3;++axis)frame.parameters.offset[axis]=calibration->positionOffset[axis];
    return frame;
}

inline ScreenObjectVisibilityView sceneReferenceObjectVisibilityView(const PreparedReferenceCameraView& camera){
    const auto& settings=camera.descriptor().objectVisibility.value();
    ScreenObjectVisibilityView result;
    result.clipToWorld=settings.calibration?settings.calibration->clipToWorld:camera.clipToReference();
    result.pixelOffset=settings.pixelOffset;
    // The reference pass reverses Y relative to the prepared camera projection.
    result.flipNdcY=true;
    result.phaseOrigin=ScreenObjectVisibilityPhaseOrigin::TopLeft;
    return result;
}

inline std::array<float,16> sceneReferenceCalibratedObjectProjection(const ReferencePositionFrame& frame,
    const SceneReferenceObjectVisibilityRecordCalibration& calibration,const internal::Matrix4& worldProjection){
    if(worldProjection==calibration.anchorWorldToUnit)return calibration.referenceToUnit;

    const auto worldFromAnchor=scene::inverseAffine(calibration.anchorWorldToUnit);
    std::array<double,16> anchorFromCurrent{};
    for(unsigned col=0;col<4;++col)for(unsigned row=0;row<4;++row)
        for(unsigned inner=0;inner<4;++inner)
            anchorFromCurrent[col*4+row]+=double(worldFromAnchor[inner*4+row])*worldProjection[col*4+inner];

    std::array<unsigned,3> worldAxes{};
    std::array<double,3> signs{};
    for(unsigned axis=0;axis<3;++axis){
        const int choice=frame.parameters.axes[axis];
        worldAxes[axis]=static_cast<unsigned>(std::abs(choice)-1);
        signs[axis]=choice<0?-1.:1.;
    }
    std::array<double,16> anchorReferenceFromCurrentReference{};
    anchorReferenceFromCurrentReference[15]=1;
    for(unsigned row=0;row<3;++row){
        double translation=signs[row]*anchorFromCurrent[12+worldAxes[row]]+frame.parameters.offset[row];
        for(unsigned col=0;col<3;++col){
            const double coefficient=signs[row]*anchorFromCurrent[worldAxes[col]*4+worldAxes[row]]*signs[col];
            anchorReferenceFromCurrentReference[col*4+row]=coefficient;
            translation-=coefficient*frame.parameters.offset[col];
        }
        anchorReferenceFromCurrentReference[12+row]=translation;
    }

    std::array<float,16> result{};
    for(unsigned col=0;col<4;++col)for(unsigned row=0;row<4;++row){
        double value=0;
        for(unsigned inner=0;inner<4;++inner)
            value+=calibration.referenceToUnit[inner*4+row]*anchorReferenceFromCurrentReference[col*4+inner];
        result[col*4+row]=static_cast<float>(value);
    }
    return result;
}

inline const SceneReferenceObjectVisibilityRecordCalibration* sceneReferenceObjectRecord(
    const SceneReferenceObjectVisibilityCalibration* calibration,unsigned page){
    if(!calibration)return nullptr;
    const auto& configured=calibration->objectRecords;
    const auto found=std::find_if(configured.begin(),configured.end(),[page](const auto& source){return source.atlasPage==page;});
    return found==configured.end()?nullptr:&*found;
}

inline std::optional<SceneReferenceObjectShadowRasterParameters> sceneReferenceObjectShadowRaster(
    const PreparedReferenceCameraView& camera,const ObjectShadowPage& page){
    const auto& settings=camera.descriptor().objectVisibility;
    if(!settings||!settings->calibration)return std::nullopt;
    const auto& calibration=*settings->calibration;
    const auto* source=sceneReferenceObjectRecord(&calibration,page.page);
    if(!source)return std::nullopt;

    const auto frame=sceneReferenceObjectVisibilityFrame(camera);
    const auto unit=sceneReferenceCalibratedObjectProjection(frame,*source,page.gpu.worldToUnitShadow);
    SceneReferenceObjectShadowRasterParameters result;
    result.raster=referenceRasterProjectionParameters(camera);
    result.raster.projection.jitter={};
    for(unsigned axis=0;axis<3;++axis){
        result.absoluteOrigin[axis]=calibration.positionOffset[axis]-result.raster.offset[axis];
        if(!std::isfinite(result.absoluteOrigin[axis]))throw std::invalid_argument("Reference object raster origin exceeds binary32");
    }
    for(unsigned col=0;col<4;++col){
        auto& column=result.raster.projection.columns[col];
        column={2*unit[col*4],-2*unit[col*4+1],unit[col*4+2],unit[col*4+3]};
    }
    result.raster.projection.columns[3][0]-=1;
    result.raster.projection.columns[3][1]+=1;
    ReferenceClipProjectionProfile profile;
    profile.parameters=result.raster.projection;
    validateReferenceClipProjection(profile);
    return result;
}

inline std::vector<ScreenObjectVisibilityRecord> sceneReferenceObjectVisibilityRecords(const ReferencePositionFrame& frame,
    const std::vector<ObjectShadowPage>& pages,const SceneReferenceObjectVisibilityCalibration* calibration=nullptr){
    static_assert(kReferenceObjectVisibilityRecordCapacity==kObjectShadowCapacity);
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
        if(const auto* source=sceneReferenceObjectRecord(calibration,page.page)){
            record.worldToUnitShadow=sceneReferenceCalibratedObjectProjection(frame,*source,worldProjection);
            record.projectionDepth=ScreenObjectDepth::Reversed;
            if(std::equal(worldProjection.begin(),worldProjection.begin()+12,source->anchorWorldToUnit.begin()))
                record.lightDirection=source->lightDirection;
        }
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
