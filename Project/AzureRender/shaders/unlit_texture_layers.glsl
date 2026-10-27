#ifndef AZURE_UNLIT_TEXTURE_LAYERS
#define AZURE_UNLIT_TEXTURE_LAYERS
vec2 azureUnlitLayerUv(vec2 uv,vec4 scaleOffset,vec4 rotation,vec2 speed,float timeSeconds) {
    vec2 d=uv+speed*timeSeconds-.5;
    vec2 rotated=vec2(d.x*rotation.x+d.y*rotation.z,d.x*rotation.y+d.y*rotation.w)+.5;
    return rotated*scaleOffset.xy+scaleOffset.zw;
}
vec4 azureUnlitLayerSample(vec4 rgba,float mode) {
    return mode>.5?vec4(1,1,1,rgba.r):rgba;
}
float azureUnlitSoftIntersection(float sceneViewZ,float effectViewZ,float distance,float bias) {
    return clamp((abs(sceneViewZ)-abs(effectViewZ)+bias)/distance,0.0,1.0);
}
float azureUnlitViewDepthFade(float effectViewZ,vec4 range,float enabled) {
    if(enabled<.5)return 1.0;
    float depth=abs(effectViewZ);
    return clamp((depth-range.x)/(range.y-range.x),0.0,1.0)
        *clamp((range.w-depth)/(range.w-range.z),0.0,1.0);
}
float azureUnlitViewCoverage(float normalDotView,vec4 parameters) {
    float power=pow(clamp(normalDotView+parameters.x,0.0,1.0),parameters.y);
    return mix(1.0,mix(1.0-power,power,parameters.z),parameters.w);
}
float azureUnlitCoverageLayer(vec4 value,float mode,vec2 remap) {
    return clamp((azureUnlitLayerSample(value,mode).a-remap.x)*remap.y,0.0,1.0);
}
vec2 azureUnlitBaseUvDistortion(vec4 sampleValue,float channels,vec4 remap) {
    return (channels>.5?sampleValue.rg:sampleValue.rr)*remap.xy+remap.zw;
}
vec4 azureUnlitLayerColor(vec4 base,vec4 second,vec4 rows[21],float soft,float exposureGain,float saturation,vec3 colorGain,vec4 vertexColor,float normalDotView,vec4 coverage) {
    base=azureUnlitLayerSample(base,rows[9].x)*rows[6];
    if(rows[11].y>.5)base*=vertexColor;
    if(rows[9].z>.5) {
        second=azureUnlitLayerSample(second,rows[9].y);
        if(rows[9].z<1.5)base*=second;
        else base.rgb+=second.rgb*rows[7].rgb*clamp((base.a+second.a)*rows[7].a,0.0,1.0);
    }
    if(rows[11].w>.5)base.a*=azureUnlitCoverageLayer(coverage,rows[15].z,rows[16].xy);
    if(rows[11].z>.5)base.a=clamp(base.a,0.0,1.0)*azureUnlitViewCoverage(normalDotView,rows[12]);
    base.rgb+=max(base.rgb-rows[8].x,vec3(0))*rows[8].y;
    base.rgb=clamp(base.rgb*mix(1.0,exposureGain,rows[8].z),vec3(0),vec3(1000));
    float luminance=dot(base.rgb,vec3(.212673,.715152,.072175));
    vec3 mapped=(luminance+saturation*(base.rgb-luminance))*colorGain;
    base.rgb=mix(base.rgb,mapped,rows[8].w);
    // Node opacity is consumed by the existing final transparent composition.
    base.a=clamp(base.a*soft,0.0,1.0);return base;
}
#endif
