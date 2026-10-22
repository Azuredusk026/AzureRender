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
// Bounded screen-space cone approximation. The source has one mip level.
// Perceptual roughness squared controls the world-space cone cross-section.
mat2 screenReflectionFootprint(mat4 worldToClip,vec3 hit,vec3 rayDirection,
    float distance,float roughness,ivec2 extent){
    vec4 clip=worldToClip*vec4(hit,1.0);
    if(clip.w<=1e-6)return mat2(0);
    vec3 ray=normalize(rayDirection);
    vec3 reference=abs(ray.z)<.9?vec3(0,0,1):vec3(0,1,0);
    vec3 tangent=normalize(cross(ray,reference)),bitangent=cross(ray,tangent);
    vec3 row0=vec3(worldToClip[0][0],worldToClip[1][0],worldToClip[2][0]);
    vec3 row1=vec3(worldToClip[0][1],worldToClip[1][1],worldToClip[2][1]);
    vec3 rowW=vec3(worldToClip[0][3],worldToClip[1][3],worldToClip[2][3]);
    vec3 dx=(row0-clip.x/clip.w*rowW)*(.5*float(extent.x)/clip.w);
    vec3 dy=(row1-clip.y/clip.w*rowW)*(.5*float(extent.y)/clip.w);
    float radius=distance*roughness*roughness;
    vec2 a=vec2(dot(dx,tangent),dot(dy,tangent))*radius;
    vec2 b=vec2(dot(dx,bitangent),dot(dy,bitangent))*radius;
    float aa=dot(a,a),bb=dot(b,b),ab=dot(a,b);
    float major=sqrt(max(0.0,.5*(aa+bb+sqrt((aa-bb)*(aa-bb)+4.0*ab*ab))));
    return mat2(a,b)*min(1.0,8.0/max(major,1e-6));
}
float screenReflectionPlaneDepth(float depth,bool perspective){
    return perspective?1.0/max(depth,1e-6):depth;
}
float screenReflectionPlaneGradient(float before,float center,float after){
    float a=center-before,b=after-center;
    float gradient=abs(a)<abs(b)?a:b;
    return abs(gradient)<=abs(center)*.1?gradient:0.0;
}
// A planar surface is affine in inverse depth for a perspective projection,
// and affine in linear depth for an orthographic projection.
vec2 screenReflectionDepthGradient(sampler2D depth,ivec2 center,float hitDepth,bool perspective){
    ivec2 last=textureSize(depth,0)-1;
    float q=screenReflectionPlaneDepth(hitDepth,perspective);
    float left=screenReflectionPlaneDepth(texelFetch(depth,clamp(center-ivec2(1,0),ivec2(0),last),0).r,perspective);
    float right=screenReflectionPlaneDepth(texelFetch(depth,clamp(center+ivec2(1,0),ivec2(0),last),0).r,perspective);
    float up=screenReflectionPlaneDepth(texelFetch(depth,clamp(center-ivec2(0,1),ivec2(0),last),0).r,perspective);
    float down=screenReflectionPlaneDepth(texelFetch(depth,clamp(center+ivec2(0,1),ivec2(0),last),0).r,perspective);
    return vec2(screenReflectionPlaneGradient(left,q,right),screenReflectionPlaneGradient(up,q,down));
}
vec4 screenReflectionFilteredRadiance(sampler2D source,sampler2D depth,mat4 worldToClip,
    vec2 uv,vec3 hit,vec3 rayDirection,float distance,float roughness,float hitDepth,float thickness){
    vec3 center=screenReflectionRadiance(source,depth,uv,hitDepth,thickness);
    // Keep the exact sharp path for zero roughness and subpixel footprints.
    if(roughness<=0.0)return vec4(center,1);
    ivec2 extent=textureSize(source,0);
    mat2 axes=screenReflectionFootprint(worldToClip,hit,rayDirection,distance,roughness,extent);
    float aa=dot(axes[0],axes[0]),bb=dot(axes[1],axes[1]),ab=dot(axes[0],axes[1]);
    float major=sqrt(max(0.0,.5*(aa+bb+sqrt((aa-bb)*(aa-bb)+4.0*ab*ab))));
    if(major<=.5)return vec4(center,1);
    bool perspective=dot(vec3(worldToClip[0][3],worldToClip[1][3],worldToClip[2][3]),
        vec3(worldToClip[0][3],worldToClip[1][3],worldToClip[2][3]))>1e-12;
    ivec2 anchor=clamp(ivec2(floor(uv*vec2(extent))),ivec2(0),extent-1);
    vec2 gradient=screenReflectionDepthGradient(depth,anchor,hitDepth,perspective);
    float planeDepth=screenReflectionPlaneDepth(hitDepth,perspective);
    const vec2 ring[12]=vec2[12](vec2(.5,0),vec2(-.5,0),vec2(0,.5),vec2(0,-.5),
        vec2(1,0),vec2(-1,0),vec2(0,1),vec2(0,-1),
        vec2(.7071067811865476,.7071067811865476),vec2(-.7071067811865476,.7071067811865476),
        vec2(.7071067811865476,-.7071067811865476),vec2(-.7071067811865476,-.7071067811865476));
    vec2 pixel=uv*vec2(extent)-.5;
    vec3 radiance=center*.2;float covered=.2;
    // At most 12 single-texel depth/color reads, plus the sharp four-texel
    // center and four depth-gradient reads: <=16 color and <=20 depth reads.
    for(int tap=0;tap<12;++tap){
        ivec2 p=ivec2(floor(pixel+axes*ring[tap]+.5));
        if(any(lessThan(p,ivec2(0)))||any(greaterThanEqual(p,extent)))continue;
        float prediction=planeDepth+dot(gradient,vec2(p-anchor));
        if(prediction<=1e-6)continue;
        float expected=perspective?1.0/prediction:prediction;
        if(abs(texelFetch(depth,p,0).r-expected)>thickness)continue;
        float weight=tap<4?.1:.05;
        radiance+=texelFetch(source,p,0).rgb*weight;covered+=weight;
    }
    float blend=smoothstep(.5,1.0,major);
    // Missing kernel energy belongs to the existing environment/probe fallback.
    return vec4(mix(center,radiance/covered,blend),mix(1.0,covered,blend));
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
            vec4 filtered=screenReflectionFilteredRadiance(source,depth,worldToClip,
                uv,point,rayDirection,high,roughness,surfaceDepth,trace.z);
            return vec4(filtered.rgb,confidence*filtered.a);
        }
        previousDelta=delta;previousDistance=distance;
    }
    return vec4(0);
}
#endif
