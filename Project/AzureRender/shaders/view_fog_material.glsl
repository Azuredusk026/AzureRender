// This entry point consumes the unlit contract without lighting-field resources.
void main(){
    PackedMaterialSurfaceProfile p=materialSurfaceProfiles.profiles[material.materialPadding>>1U];
    vec4 baseColor;vec4 color=azureSampleUnlitSurface(p,baseColor);
    float depth=dot(worldPosition-camera.cameraPosition.xyz,camera.cameraForward.xyz);
    if(p.rows[16].w>.5)color.rgb=azureApplyFogTransport(azureSampleViewFog(AZURE_FRAGMENT_COORD.xy,depth),color.rgb,p.rows[15].w>.5?0.0:1.0);
    int effect=int(floor(camera.qaParameters.y+.5));
    if(effect==6&&camera.qaParameters.z<.5)color.rgb=vec3(0);
    int isolation=int(floor(camera.qaParameters.x+.5));
    vec3 normal=normalize(worldNormal);
    vec3 diagnostic=color.rgb;
    if(isolation==1)diagnostic=baseColor.rgb;
    else if(isolation==3||isolation==23)diagnostic=vec3(1);
    else if(isolation==8)diagnostic=materialClassColor(AZURE_MATERIAL_CLASS);
    else if(isolation==9)diagnostic=vec3(texture(AZ_TEX_STYLE_MASK,textureCoordinate).r);
    else if(isolation==21)diagnostic=normal*.5+.5;
    else if(isolation!=0&&isolation!=7)diagnostic=vec3(0);
    outputColor=vec4(diagnostic,color.a);
    if((material.materialPadding&AZURE_SELECTED_PADDING)!=0U){outputColor.rgb=mix(outputColor.rgb,vec3(.96,.62,.10),.42);outputColor.a=1;}
    outputColor.a*=material.nodeOpacity;
    float participation=(1.0-clamp(material.showcasePlatform,0.0,1.0))*material.featureParameters.x
        *mix(1.0,.68,clamp(material.matcapColor.a,0.0,1.0));
    if(camera.clusterLighting.x>.5)participation=0;
    outputNormal=vec4(normal*.5+.5,participation);
}
