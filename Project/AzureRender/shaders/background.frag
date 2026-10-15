#version 450

layout(binding = 0) uniform CameraData {
    vec4 cameraPosition;
    vec4 cameraForward;
    vec4 clusterGrid;
    vec4 clusterDepth;
    vec4 clusterLighting;
    vec4 cascadeSplits;
    vec4 renderingParameters;
    vec4 showcaseParameters;
    vec4 qaParameters;
    vec4 faceLightDirection;
    vec4 faceSdfParameters;
    vec4 faceSdfShadowColor;
    vec4 mainLightDirection;
    vec4 cascadeDepthRanges;
    vec4 skyRight;
    vec4 skyUp;
    vec4 skyForward;
    vec4 skyColor;
    vec4 skyParameters;
} camera;

// The background samples the equirect environment so external HDR assets are
// visible in the backdrop. Bindless layout: slot 0 of the global array.
#if defined(AZURE_BINDLESS)
layout(binding = 1) uniform sampler2D textureArray[];
#define AZ_TEX_ENVIRONMENT textureArray[0u]
#else
layout(binding = 4) uniform sampler2D environmentTexture;
#define AZ_TEX_ENVIRONMENT environmentTexture
#endif

layout(location = 0) in vec2 screenUv;
layout(location = 0) out vec4 outputColor;
layout(location = 1) out vec4 outputNormal;

vec2 directionToEquirectangular(vec3 direction) {
    const float pi = 3.14159265358979323846;
    vec3 normalizedDirection = normalize(direction);
    float u = atan(normalizedDirection.z, normalizedDirection.x) * 0.5 / pi + 0.5;
    float v = acos(clamp(normalizedDirection.y, -1.0, 1.0)) / pi;
    return vec2(u, v);
}

// Infinite sky uses orientation and projection, never camera translation.
void main() {
    vec3 direction=normalize(camera.skyForward.xyz
        +(screenUv.x*2.0-1.0)*camera.skyRight.xyz
        +(1.0-screenUv.y*2.0)*camera.skyUp.xyz);
    float angle=camera.skyParameters.z;
    direction.xz=mat2(cos(angle),sin(angle),-sin(angle),cos(angle))*direction.xz;
    vec3 color;
    if(camera.skyParameters.x>1.5){
        color=camera.skyColor.rgb;
    }else if(camera.skyParameters.x>.5){
        color=texture(AZ_TEX_ENVIRONMENT,directionToEquirectangular(direction)).rgb;
    }else{
        // Linear HDR colors: a warm horizon, blue zenith, muted ground and sun.
        float elevation=max(direction.y,0.0);
        vec3 horizon=vec3(.54,.66,.78);
        vec3 zenith=vec3(.055,.17,.38);
        vec3 sky=mix(horizon,zenith,pow(elevation,.45));
        vec3 sunDirection=normalize(vec3(.48,.72,.5));
        float alignment=max(dot(direction,sunDirection),0.0);
        sky+=vec3(1.0,.66,.3)*pow(alignment,24.0)*.35;
        sky+=vec3(8.0,6.4,4.5)*smoothstep(.9995,.99985,alignment);
        vec3 ground=mix(vec3(.12,.14,.17),horizon,exp(min(direction.y,0.0)*18.0));
        color=mix(ground,sky,smoothstep(-.025,.025,direction.y));
    }
    outputColor=vec4(color*camera.skyParameters.y,1.0);
    outputNormal=vec4(.5,.5,1.0,0.0);
}
