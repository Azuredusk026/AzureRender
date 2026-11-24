#ifndef AZURE_REFLECTION_PROBE_FIELD_GLSL
#define AZURE_REFLECTION_PROBE_FIELD_GLSL
#include "octahedral_environment.glsl"
// probeFieldRows is a readonly storage block supplied by the caller.
float probeFieldAdapt(vec4 sh,vec3 normal,float luminance){
    float reference=max(dot(sh,vec4(normal,1)),.0001);
    float ratio=(luminance+2*min(abs(luminance/reference),1.0))/(reference+2);
    return fma(ratio-1,probeFieldRows[1].z,1.0);
}
vec4 reflectionProbeFieldRadiance(sampler2DArray source,vec3 position,vec3 direction,vec3 normal,float luminance,float lod){
    mat3 fieldMapping=transpose(mat3(probeFieldRows[4].xyz,probeFieldRows[5].xyz,probeFieldRows[6].xyz));
    position=fieldMapping*position;direction=fieldMapping*direction;normal=fieldMapping*normal;
    vec3 color=vec3(0);float remaining=1;
    for(uint i=0;i<uint(probeFieldRows[1].x)&&remaining>.01;++i){
        uint first=7+7*i;
        vec4 extent=probeFieldRows[first+1],blend=probeFieldRows[first+5],flags=probeFieldRows[first+6];
        vec3 local=vec3(dot(probeFieldRows[first+2],vec4(position,1)),
            dot(probeFieldRows[first+3],vec4(position,1)),dot(probeFieldRows[first+4],vec4(position,1)));
        if(any(greaterThan(abs(local),extent.xyz)))continue;
        vec3 edges=(extent.xyz-abs(local))*blend.xyz;
        float box=min(edges.x,min(edges.y,edges.z));
        // Keep the source radial arithmetic before interpolation of the shape.
        precise float radius=extent.x*.1;
        precise vec3 radial=abs(local)*.1;
        precise vec2 squared=radial.xy*radial.xy;
        precise float distanceSquared=fma(radial.z,radial.z,squared.x+squared.y);
        precise float sphere=fma(radius,radius,-distanceSquared)*blend.x;
        sphere=sphere*blend.x;
        sphere=sphere*(1-flags.y);
        sphere=sphere*100;
        float weight=clamp(fma(box,flags.y,sphere),0,1)*flags.w;
        if(weight<=0)continue;
        vec3 sampleDirection=direction;
        if(flags.x>.5){
            vec3 ray=vec3(dot(probeFieldRows[first+2].xyz,direction),
                dot(probeFieldRows[first+3].xyz,direction),dot(probeFieldRows[first+4].xyz,direction));
            vec3 distances=vec3(3.402823e38);
            for(int axis=0;axis<3;++axis)if(ray[axis]!=0)
                distances[axis]=((ray[axis]>0?extent[axis]:-extent[axis])-local[axis])/ray[axis];
            sampleDirection=fma(ray,vec3(min(distances.x,min(distances.y,distances.z))),local);
        }
        vec2 uv=octahedralEnvironmentUv(sampleDirection,probeFieldRows[2],probeFieldRows[3].x>.5);
        vec3 radiance=textureLod(source,vec3(uv,extent.w),lod).rgb*blend.w;
        radiance*=probeFieldAdapt(probeFieldRows[first],normal,luminance);
        color=fma(radiance*weight,vec3(remaining),color);
        remaining*=1-weight;
    }
    if(remaining>.01){
        vec2 uv=octahedralEnvironmentUv(direction,probeFieldRows[2],probeFieldRows[3].x>.5);
        vec3 radiance=textureLod(source,vec3(uv,probeFieldRows[1].y),lod).rgb;
        radiance*=probeFieldAdapt(probeFieldRows[0],normal,luminance);
        color=fma(radiance,vec3(remaining),color);
    }
    return vec4(color*probeFieldRows[1].w,remaining);
}
#endif
