#define AZURE_FACE_SDF_V2 1
#define AZURE_FACE_SDF_WORLD_NORMAL 1

// The strip stores RGB colors in sRGB; sampler decoding precedes filtering.
// Coordinates also use sRGB. Red runs within a slice, blue across slices,
// and green along rows. The last blue slice clamps to itself at the endpoint.
vec3 sampleShadowColorLut(sampler2D lut,vec3 linearColor) {
    vec3 value=max(linearColor,vec3(0));
    vec3 encoded=clamp(mix(1.055*pow(value,vec3(1.0/2.4))-.055,
        12.92*value,lessThanEqual(value,vec3(.0031308))),0.0,1.0);
    float dimension=float(textureSize(lut,0).y);
    float slice=encoded.b*(dimension-1.0);
    float first=floor(slice);
    vec2 uv=vec2((first*dimension+encoded.r*(dimension-1.0)+.5)/(dimension*dimension),
        (encoded.g*(dimension-1.0)+.5)/dimension);
    vec2 next=uv+vec2(1.0/dimension,0);
    return mix(textureLod(lut,uv,0).rgb,textureLod(lut,next,0).rgb,fract(slice));
}

vec2 faceSdfSampleUv(vec2 uv,float localLightX) {
    return vec2(localLightX>0.0?uv.x:1.0-uv.x,uv.y);
}
vec3 faceSdfNormal(vec4 lightmap,float localLightX) {
    float nx=(lightmap.b*2.0-1.0)*(localLightX>0.0?1.0:-1.0);
    return normalize(vec3(nx,6.103515625e-5,1.0-abs(nx)));
}
vec3 faceSdfWorldNormal(vec4 lightmap,float localLightX,mat3 headFrame,
    vec3 geometryNormal,float geometryMix) {
    vec3 worldNormal=normalize(headFrame*faceSdfNormal(lightmap,localLightX));
    vec3 blendedNormal=mix(worldNormal,geometryNormal,geometryMix);
    return dot(blendedNormal,blendedNormal)>1e-12
        ? normalize(blendedNormal) : geometryNormal;
}
// Inputs are in an orthonormal head frame. Vertical lights/cameras have a
// defined horizontal fallback so poles cannot introduce NaNs into the ramp.
float faceSdfRampCoordinate(vec4 lightmap,float geometryMix,vec3 localLight,
    vec3 localCamera,float geometricNdotL,vec3 adjustment) {
    vec3 horizontal=normalize(vec3(localLight.x,6.103515625e-5,localLight.z));
    vec2 lightPlane=length(localLight.xz)>1e-6?normalize(localLight.xz):vec2(0,1);
    vec2 cameraPlane=length(localCamera.xz)>1e-6?normalize(localCamera.xz):vec2(0,1);
    float back=clamp(-dot(lightPlane,cameraPlane),0.0,1.0)
        *clamp(-horizontal.z,0.0,1.0)*(1.0-adjustment.x);
    float halfWrap=.5*clamp(horizontal.z+back*.5*(1.0-horizontal.z*horizontal.z)+adjustment.z,-1.0,1.0);
    float threshold=clamp(.5-halfWrap,.001,.999);
    float p=smoothstep(max(2.0*threshold-1.0,0.0),min(2.0*threshold,1.0),.5*(lightmap.r+lightmap.g));
    float sdf=2.0*abs(p+halfWrap*ceil(halfWrap))-1.0;
    float geometry=clamp(geometricNdotL+adjustment.y*adjustment.x,-1.0,1.0);
    return clamp(.5+.5*mix(sdf,geometry,geometryMix),0.0,1.0);
}

float dielectricF0(float specular) {
    // UE's authored dielectric specular uses 0.5 for the usual 4% reflectance.
    return 0.08 * clamp(specular, 0.0, 1.0);
}

float sceneRefractionWeight(float alpha,float contribution) {
    return clamp((1.0-alpha)/max(1.0-contribution,.01),0.0,1.0);
}
vec2 sceneRefractionOffset(vec2 map,float strength) {return (map*2.0-1.0)*strength;}
vec2 sceneRefractionUv(vec2 uv,vec2 offset,float fullDepth,float quarterDepth,float surfaceDepth) {
    if(fullDepth>=surfaceDepth)return uv+offset;
    if(quarterDepth>=surfaceDepth)return uv+offset*.25;
    return uv;
}
vec2 sceneRefractionMirrorUv(vec2 uv) {return 1.0-abs(mod(uv,2.0)-1.0);}
vec3 sceneRefractionSurface(vec3 diffuse,vec3 specular,vec3 background,float alpha,float contribution) {
    return diffuse+mix(specular,background,sceneRefractionWeight(alpha,contribution));
}

vec3 toonIndirectChroma(vec3 ramp, float toonWeight, uint materialClass) {
    if (materialClass != 1U && materialClass != 2U) return vec3(1.0);
    float peak = max(max(ramp.r, ramp.g), ramp.b);
    if (peak <= 0.0001) return vec3(1.0);
    // Indirect illumination keeps its energy and direction; the ramp supplies
    // part of its chroma so neutral fill cannot erase authored skin warmth.
    return mix(vec3(1.0), ramp / peak, 0.5 * clamp(toonWeight, 0.0, 1.0));
}

vec3 directSurfaceSpecular(vec3 f0, float roughness, float nl, float nv, float nh, float vh) {
    if (nl <= 0.0 || nv <= 0.0) return vec3(0.0);
    float alpha = max(roughness * roughness, 0.001);
    float alphaSquared = alpha * alpha;
    float denominator = nh * nh * (alphaSquared - 1.0) + 1.0;
    float distribution = alphaSquared / (3.141592653589793 * denominator * denominator);
    float lambdaView = nl * sqrt(nv * nv * (1.0 - alphaSquared) + alphaSquared);
    float lambdaLight = nv * sqrt(nl * nl * (1.0 - alphaSquared) + alphaSquared);
    float visibility = 0.5 / max(lambdaView + lambdaLight, 0.00001);
    vec3 fresnel = f0 + (vec3(1.0) - f0) * pow(1.0 - clamp(vh, 0.0, 1.0), 5.0);
    return distribution * visibility * fresnel * nl;
}
