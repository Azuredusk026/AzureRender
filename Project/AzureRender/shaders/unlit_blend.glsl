bool azureWriteUnlitBlendBeauty(vec4 baseColor,vec3 geometricNormal,float platformMask,vec3 snapshotNormal,float roughness){
    uint features=AZURE_MATERIAL_FEATURES;
    if(AZURE_ALPHA_MODE!=2||(AZURE_MATERIAL_CLASS!=0U&&AZURE_MATERIAL_CLASS!=8U)
        ||(features&262144U)==0U||(features&~0xc1040028U)!=0U)return false;
    int isolation=int(floor(AZURE_QA_ISOLATION+.5)),effect=int(floor(camera.qaParameters.y+.5));
#if defined(AZURE_OPAQUE_SNAPSHOT)
    if(camera.screenReflectionTrace.x>.5){isolation=0;effect=0;}
#endif
    if(isolation!=0||effect!=0)return false;
    vec3 color=baseColor.rgb+texture(AZ_TEX_SPECULAR_EMISSIVE,textureCoordinate).rgb
        *material.emissiveStrength*materialFeatureEnabled(8U);
    float alpha=baseColor.a;
    if((features&16777216U)!=0U){
        PackedMaterialSurfaceProfile p=materialSurfaceProfiles.profiles[material.materialPadding>>1U];
        vec4 layered=azureSampleUnlitSurface(p,baseColor);
        color=layered.rgb;alpha=layered.a;
        if(camera.nprLightingParameters.x>.5&&p.rows[15].w>1.5){
            uint gridX=max(uint(camera.clusterGrid.x),1U);
            uint gridY=max(uint(camera.clusterGrid.y),1U);
            uint gridZ=max(uint(camera.clusterGrid.z),1U);
            vec2 screenUv=AZURE_FRAGMENT_COORD.xy/max(camera.clusterDepth.zw,vec2(1.0));
            uint clusterX=min(uint(clamp(screenUv.x,0.0,.999999)*float(gridX)),gridX-1U);
            uint clusterY=min(uint(clamp(screenUv.y,0.0,.999999)*float(gridY)),gridY-1U);
            float nearDepth=max(camera.clusterDepth.x,.001);
            float farDepth=max(camera.clusterDepth.y,nearDepth+.001);
            float viewDepth=max(dot(worldPosition-camera.cameraPosition.xyz,camera.cameraForward.xyz),nearDepth);
            float logarithmicDepth=log(viewDepth/nearDepth)/log(farDepth/nearDepth);
            uint clusterZ=min(uint(clamp(logarithmicDepth,0.0,.999999)*float(gridZ)),gridZ-1U);
            uvec2 range=clusterHeaderData.clusters[(clusterZ*gridY+clusterY)*gridX+clusterX];
            uint count=uint(max(camera.clusterGrid.w,0.0));
            float attenuation=1.0;
            for(uint offset=0U;offset<range.y;++offset){
                uint index=clusterIndexData.indices[range.x+offset];
                if(index>=count)continue;
                ClusterLight light=sceneLightData.lights[index];
                bool blend=light.characterResponse.x==2.0||light.characterResponse.x==3.0;
                if(!blend||(uint(light.boxParameters.w)&2U)==0U)continue;
                SceneLightSample lightSample=sampleSceneLight(light,worldPosition,light.characterResponse.x==3.0);
                if(lightSample.attenuation>.0001){
                    float weight=characterColorBlendWeight(lightSample.attenuation,dot(geometricNormal,lightSample.direction),light.characterResponse.yz);
                    attenuation*=1.0-weight;
                }
            }
            if(attenuation<1.0)color=color*attenuation+vec3(0);
        }
    }
    float radianceScale=azureMaterialRadianceScale();
    if(radianceScale!=1.0)color*=radianceScale;
    outputColor=vec4(color,alpha);
    if((material.materialPadding&AZURE_SELECTED_PADDING)!=0U){
        outputColor.rgb=mix(outputColor.rgb,vec3(.96,.62,.10),.42);outputColor.a=1.0;
    }
    outputColor.a*=material.nodeOpacity;
#if defined(AZURE_OPAQUE_SNAPSHOT)
    outputNormal=encodeReflectionSurface(dot(worldPosition-camera.cameraPosition.xyz,camera.cameraForward.xyz),snapshotNormal,AZURE_MATERIAL_CLASS==0U?roughness:1.0);
#else
    azureWriteDisplayMask(outputColor.a);
    float participation=(1-platformMask)*material.featureParameters.x
        *mix(1.0,.68,clamp(material.matcapColor.a,0.0,1.0));
    if(camera.clusterLighting.x>.5)participation=0;
    outputNormal=vec4(geometricNormal*.5+.5,participation);
#endif
    return true;
}
