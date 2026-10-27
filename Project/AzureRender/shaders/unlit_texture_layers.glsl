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
vec4 azureUnlitLayerColor(vec4 base,vec4 second,vec4 rows[12],float soft,float exposureGain,float saturation,vec3 colorGain) {
    base=azureUnlitLayerSample(base,rows[9].x)*rows[6];
    if(rows[9].z>.5) {
        second=azureUnlitLayerSample(second,rows[9].y);
        if(rows[9].z<1.5)base*=second;
        else base.rgb+=second.rgb*rows[7].rgb*clamp((base.a+second.a)*rows[7].a,0.0,1.0);
    }
    base.rgb+=max(base.rgb-rows[8].x,vec3(0))*rows[8].y;
    base.rgb=clamp(base.rgb*mix(1.0,exposureGain,rows[8].z),vec3(0),vec3(1000));
    float luminance=dot(base.rgb,vec3(.212673,.715152,.072175));
    vec3 mapped=(luminance+saturation*(base.rgb-luminance))*colorGain;
    base.rgb=mix(base.rgb,mapped,rows[8].w);
    // Node opacity is consumed by the existing final transparent composition.
    base.a=clamp(base.a*soft,0.0,1.0);return base;
}
#endif
