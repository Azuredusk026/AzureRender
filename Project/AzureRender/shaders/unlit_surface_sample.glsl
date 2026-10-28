#ifndef AZURE_UNLIT_SURFACE_SAMPLE_GLSL
#define AZURE_UNLIT_SURFACE_SAMPLE_GLSL
vec4 azureSampleUnlitSurface(PackedMaterialSurfaceProfile p,out vec4 baseColor){
    float tau=p.rows[5].x*camera.unlitTimeExposure.x+p.rows[5].y;
    vec2 baseUv=azureUnlitLayerUv(textureCoordinate,p.rows[0],p.rows[1],p.rows[2].xy,tau);
    if(p.rows[16].z>.5) {
        vec4 noise=texture(AZ_TEX_SPECULAR_EMISSIVE,azureUnlitLayerUv(textureCoordinate,p.rows[17],p.rows[18],p.rows[19].xy,tau));
        baseUv+=azureUnlitBaseUvDistortion(noise,p.rows[19].z,p.rows[20]);
    }
    vec4 layerBase=texture(AZ_TEX_BASE_COLOR,baseUv);
    vec4 second=vec4(1);
    if(p.rows[9].z>.5)second=texture(AZ_TEX_STYLE_MASK,azureUnlitLayerUv(textureCoordinate,p.rows[3],p.rows[4],p.rows[2].zw,tau));
    float soft=1.0;
    float effectDepth=dot(worldPosition-camera.cameraPosition.xyz,camera.cameraForward.xyz);
#if !defined(AZURE_OPAQUE_SNAPSHOT)
    if(p.rows[9].w>.5) {
        float sceneDepth=texture(overlaySceneDepth,AZURE_FRAGMENT_COORD.xy/camera.clusterDepth.zw).r;
        soft=azureUnlitSoftIntersection(sceneDepth,effectDepth,p.rows[5].z,p.rows[5].w);
    }
#endif
    soft*=azureUnlitViewDepthFade(effectDepth,p.rows[10],p.rows[11].x);
    vec4 coverage=vec4(1);
    if(p.rows[11].w>.5)coverage=texture(AZ_TEX_SHADOW_COLOR_LUT,azureUnlitLayerUv(textureCoordinate,p.rows[13],p.rows[14],p.rows[15].xy,tau));
    vec3 projectiveRow=vec3(camera.screenReflectionWorldToClip[0].w,camera.screenReflectionWorldToClip[1].w,camera.screenReflectionWorldToClip[2].w);
    vec3 viewDirection=dot(projectiveRow,projectiveRow)<1e-12?-camera.cameraForward.xyz:normalize(camera.cameraPosition.xyz-worldPosition);
    vec4 layered=azureUnlitLayerColor(layerBase,second,p.rows,soft,camera.unlitTimeExposure.y,camera.unlitTimeExposure.z,camera.unlitColorGain.rgb,vec4(vertexColorRgb,vertexColorAlpha),dot(worldNormal,viewDirection),coverage);
    baseColor=azureUnlitLayerSample(layerBase,p.rows[9].x)*p.rows[6];
    if(p.rows[11].y>.5)baseColor*=vec4(vertexColorRgb,vertexColorAlpha);
    return layered;
}
#endif
