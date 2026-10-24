#ifndef AZURE_MATERIAL_SAMPLING_GLSL
#define AZURE_MATERIAL_SAMPLING_GLSL
const uint azurePixelIntegralFeature=8388608u;
// Spatial extrapolation is restricted to a locally constant shading frame.
// Varying frames use the geometry actually rasterized by temporal samples.
bool azureUniformMaterialFrame(vec3 nx,vec3 ny,vec3 tx,vec3 ty){
    return length(nx)+length(ny)+length(tx)+length(ty)<=1e-6;
}
float azureTemporalBlendAlpha(sampler2D source,vec2 uv,vec2 dx,vec2 dy,vec2 jitter){
    float alpha=0;
    for(int y=0;y<4;++y)for(int x=0;x<4;++x){
        vec2 offset=(vec2(x,y)+.5)*.25-.5+jitter;
        alpha+=textureGrad(source,uv+dx*offset.x+dy*offset.y,dx*.25,dy*.25).a;
    }
    return alpha*(1.0/16.0);
}
#endif
