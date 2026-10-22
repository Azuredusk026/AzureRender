#ifndef AZURE_SCREEN_SPACE_REFLECTION_GLSL
#define AZURE_SCREEN_SPACE_REFLECTION_GLSL
bool screenReflectionProject(mat4 worldToClip,vec3 position,out vec2 uv){
    vec4 clip=worldToClip*vec4(position,1.0);
    if(clip.w<=1e-6||clip.z<0.0||clip.z>clip.w)return false;
    uv=clip.xy/clip.w*.5+.5;
    return all(greaterThanEqual(uv,vec2(0)))&&all(lessThanEqual(uv,vec2(1)));
}
float screenReflectionConfidence(vec2 uv,float roughness,float distance,vec4 trace,vec4 quality){
    float edge=min(min(uv.x,1.0-uv.x),min(uv.y,1.0-uv.y));
    return smoothstep(0.0,quality.z,edge)
        *(1.0-smoothstep(quality.y*.75,quality.y,roughness))
        *(1.0-smoothstep(trace.y*.75,trace.y,distance))*quality.w;
}
// Source sampler is shared with depth and uses nearest filtering. Four taps
// interpolate radiance explicitly without averaging unrelated depth surfaces.
vec3 screenReflectionRadiance(sampler2D source,sampler2D depth,vec2 uv,float hitDepth,float thickness){
    ivec2 extent=textureSize(source,0);vec2 pixel=uv*vec2(extent)-.5;
    ivec2 base=ivec2(floor(pixel));vec2 fraction=fract(pixel);
    vec3 radiance=vec3(0);float total=0.0;
    for(int y=0;y<2;++y)for(int x=0;x<2;++x){
        ivec2 p=clamp(base+ivec2(x,y),ivec2(0),extent-1);
        float w=(x==0?1.0-fraction.x:fraction.x)*(y==0?1.0-fraction.y:fraction.y);
        if(abs(texelFetch(depth,p,0).r-hitDepth)<=thickness){radiance+=texelFetch(source,p,0).rgb*w;total+=w;}
    }
    return total>1e-6?radiance/total:texture(source,uv).rgb;
}
vec4 traceScreenSpaceReflection(sampler2D source,sampler2D depth,mat4 worldToClip,
    vec3 cameraPosition,vec3 cameraForward,vec3 worldPosition,vec3 normal,vec3 rayDirection,
    float roughness,vec4 trace,vec4 quality){
    if(trace.x<.5||roughness>=quality.y)return vec4(0);
    vec3 origin=worldPosition+normal*trace.w;
    vec2 uv;float previousDistance=0.0;
    if(!screenReflectionProject(worldToClip,origin,uv))return vec4(0);
    float previousDelta=dot(origin-cameraPosition,cameraForward)-texture(depth,uv).r;
    if(previousDelta>=0.0)return vec4(0); // The biased origin must be in visible free space.
    int steps=clamp(int(quality.x+.5),8,256);
    for(int i=1;i<=256;++i){
        if(i>steps)break;
        float distance=trace.y*float(i)/float(steps);
        vec3 point=origin+rayDirection*distance;
        if(!screenReflectionProject(worldToClip,point,uv))return vec4(0);
        float surfaceDepth=texture(depth,uv).r;
        float delta=dot(point-cameraPosition,cameraForward)-surfaceDepth;
        if(previousDelta<0.0&&delta>=0.0){
            float low=previousDistance,high=distance;
            for(int refine=0;refine<6;++refine){
                float middle=(low+high)*.5;vec3 candidate=origin+rayDirection*middle;vec2 candidateUv;
                if(!screenReflectionProject(worldToClip,candidate,candidateUv))return vec4(0);
                float separation=dot(candidate-cameraPosition,cameraForward)-texture(depth,candidateUv).r;
                if(separation>=0.0)high=middle;else low=middle;
            }
            point=origin+rayDirection*high;
            if(!screenReflectionProject(worldToClip,point,uv))return vec4(0);
            surfaceDepth=texture(depth,uv).r;
            delta=dot(point-cameraPosition,cameraForward)-surfaceDepth;
            if(surfaceDepth<=0.0||delta<0.0||delta>trace.z)return vec4(0);
            float confidence=screenReflectionConfidence(uv,roughness,high,trace,quality);
            return vec4(screenReflectionRadiance(source,depth,uv,surfaceDepth,trace.z),confidence);
        }
        previousDelta=delta;previousDistance=distance;
    }
    return vec4(0);
}
#endif
