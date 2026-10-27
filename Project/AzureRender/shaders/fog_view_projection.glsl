#ifndef AZURE_FOG_VIEW_PROJECTION_GLSL
#define AZURE_FOG_VIEW_PROJECTION_GLSL
vec4 azureFogViewCoordinates(vec4 rows[3],uvec2 pixel,float viewDepth) {
    uint flags=uint(rows[2].w);
    if((flags&8U)!=0U)pixel.y=uint(rows[2].z)-1U-pixel.y;
    uvec3 hash=uvec3(pixel,flags&7U)*1664525U+1013904223U;
    hash.x+=hash.y*hash.z;hash.y+=hash.z*hash.x;hash.z+=hash.x*hash.y;
    hash.x+=hash.y*hash.z;hash.y+=hash.z*hash.x;
    vec2 jitter=vec2(hash.xy>>16U)/32768.0-1.0;
    vec2 uv=fma(jitter,vec2(rows[0].z),vec2(pixel)+rows[0].w)*rows[0].xy;
    float depth=abs(viewDepth);
    float z=log2(fma(depth,rows[1].x,rows[1].y))*rows[1].z;
    return vec4(uv,z,clamp((depth-rows[1].w)/rows[2].x,0.0,1.0));
}
#endif
