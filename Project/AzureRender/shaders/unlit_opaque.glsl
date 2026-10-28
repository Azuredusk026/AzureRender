// The plain opaque unlit contract can emit beauty before lighting evaluation.
// Other diagnostics and feature combinations retain their complete entry path.
bool azureWritePlainUnlitBeauty(vec4 baseColor,vec3 geometricNormal,float platformMask){
    uint features=AZURE_MATERIAL_FEATURES;
    if(AZURE_ALPHA_MODE!=0||(AZURE_MATERIAL_CLASS!=0U&&AZURE_MATERIAL_CLASS!=8U)
        ||(features&262144U)==0U||(features&~0xc0040028U)!=0U)return false;
    int isolation=int(floor(camera.qaParameters.x+.5));
    int effect=int(floor(camera.qaParameters.y+.5));
    bool disabled=camera.qaParameters.z<.5;
#if defined(AZURE_OPAQUE_SNAPSHOT)
    if(camera.screenReflectionTrace.x>.5){isolation=0;effect=0;disabled=false;}
#endif
    if(isolation!=0)return false;
    vec3 emission=texture(AZ_TEX_SPECULAR_EMISSIVE,textureCoordinate).rgb
        *material.emissiveStrength*float((features&8U)!=0U);
    if(effect==6&&disabled)emission=vec3(0);
    outputColor=vec4(baseColor.rgb+emission,1);
    if((material.materialPadding&AZURE_SELECTED_PADDING)!=0U)
        outputColor.rgb=mix(outputColor.rgb,vec3(.96,.62,.10),.42);
#if defined(AZURE_OPAQUE_SNAPSHOT)
    outputNormal=vec4(dot(worldPosition-camera.cameraPosition.xyz,camera.cameraForward.xyz),0,0,1);
#else
    float participation=(1-platformMask)*material.featureParameters.x
        *mix(1.0,.68,clamp(material.matcapColor.a,0.0,1.0));
    if(camera.clusterLighting.x>.5)participation=0;
    outputNormal=vec4(geometricNormal*.5+.5,participation);
#endif
    return true;
}
