#include "reference_raster_projection.glsl"
vec4 azureCameraRasterPosition(vec3 world,vec4 deviceBase,vec4 deviceFinal){
    return azureReferenceRasterPosition(world,deviceBase,deviceFinal,
        camera.referenceColumns,camera.referenceJitter.xy,camera.referenceControl.y,
        camera.referenceOffset.xyz,(camera.referenceControl.z&1u)!=0u,(camera.referenceControl.z&2u)!=0u);
}
