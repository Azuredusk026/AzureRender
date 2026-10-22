// Shared strand response. Light energy and color belong to the caller.
vec2 characterHairLobes(vec3 normal, vec3 tangent, vec3 bitangent,
    vec3 highlightNormal, vec3 view, vec3 light, vec3 viewOffset,
    float power, float strandShift) {
    float shift=dot(highlightNormal,tangent)*.16;
    vec3 primary=normalize(bitangent+normal*shift);
    vec3 secondary=normalize(bitangent+normal*(shift+strandShift));
    vec3 halfVector=normalize(light+normalize(view+viewOffset));
    vec2 projection=vec2(dot(primary,halfVector),dot(secondary,halfVector));
    vec2 sine=sqrt(max(vec2(1)-projection*projection,vec2(0)));
    float exponent=clamp(power*.40,80.0,220.0);
    return pow(sine,vec2(exponent,clamp(exponent*.42,38.0,96.0)));
}

// Versioned material classes select the response independently of the level.
bool sceneCharacterSurface(uint materialClass,uint profileVersion) {
    return profileVersion>=2U && materialClass>=1U && materialClass<=6U;
}

// Projected strand response. The caller owns the half-vector policy and light
// energy. Shifts are signed: 2 * authored anisotropy value - 1.
struct CharacterProjectedHairCoordinates {
    vec2 uv;
    float edgeFade;
    float secondaryLobe;
};

struct CharacterProjectedHairEnergy {
    vec3 q;
    vec3 primary;
    vec3 secondary;
};

vec3 characterProjectedSafeNormalize(vec3 value, vec3 fallback) {
    float length2 = dot(value, value);
    return length2 > 1e-12 ? value * inversesqrt(length2) : fallback;
}

vec3 characterProjectedVirtualHalf(vec3 view,vec3 light,vec3 objectX,vec3 objectY,vec3 objectZ,float blend){
    vec3 local=vec3(dot(view,objectX),mix(.5,light.y,blend),dot(view,objectZ));
    vec3 virtualLight=characterProjectedSafeNormalize(2.0*(objectX*local.x+objectY*local.y+objectZ*local.z)+blend*light,vec3(0,0,1));
    vec3 halfVector=virtualLight+view;
    return halfVector*inversesqrt(max(dot(halfVector,halfVector),1.0/16384.0));
}

vec3 characterProjectedHairNormal(vec2 encoded,float scale,vec3 normal,vec4 tangent){
    vec2 xy=vec2(encoded.x*2.0-1.0,1.0-encoded.y*2.0);
    float z=sqrt(max(1.0-min(dot(xy,xy),1.0),0.0));
    vec3 bitangent=cross(normal,tangent.xyz)*tangent.w;
    return characterProjectedSafeNormalize(tangent.xyz*xy.x*scale+bitangent*xy.y*scale+normal*z,
        characterProjectedSafeNormalize(normal,vec3(0,0,1)));
}

vec3 characterProjectedHairStrand(vec3 normal,vec4 tangent,float parameterR,vec3 objectX,vec3 objectY,vec3 objectZ,float directionX){
    vec3 axis=characterProjectedSafeNormalize(objectX*directionX+objectY,vec3(0,1,0));
    float orientation=dot(cross(objectX,objectY),objectZ)<0.0?-1.0:1.0;
    vec3 blend=mix(orientation*cross(normal,axis),tangent.xyz,parameterR);
    return orientation*cross(normal,blend)*mix(1.0,tangent.w*orientation,parameterR);
}

float characterProjectedHairSine(float projection) {
    return max(sqrt(max(1.0 - projection * projection, 0.0)), 0.0001);
}

CharacterProjectedHairCoordinates characterProjectedHairCoordinates(
    vec3 halfVector, vec3 highlightNormal, vec3 view, vec4 sourceParameters,
    vec4 tangent, vec3 objectX, vec3 objectY, vec3 objectZ,
    float primaryShift, float secondaryShift, float range2,
    float edgeFadePower, float directionX) {
    vec3 normal = characterProjectedSafeNormalize(highlightNormal, vec3(0, 0, 1));
    vec3 viewDirection = characterProjectedSafeNormalize(view, vec3(0, 0, 1));
    vec3 halfDirection = characterProjectedSafeNormalize(halfVector, vec3(0, 0, 1));
    vec3 strand = characterProjectedHairStrand(normal,tangent,sourceParameters.r,objectX,objectY,objectZ,directionX);
    vec3 fallback = characterProjectedSafeNormalize(tangent.xyz, vec3(1, 0, 0));
    vec3 primary = characterProjectedSafeNormalize(
        strand + normal * primaryShift, fallback);
    vec3 secondary = characterProjectedSafeNormalize(
        strand + normal * secondaryShift, fallback);
    vec2 normalProjection = vec2(dot(normal, objectX), dot(normal, objectZ));
    vec2 viewProjection = vec2(dot(viewDirection, objectX), dot(viewDirection, objectZ));
    float normalLength2 = dot(normalProjection, normalProjection);
    float viewLength2 = dot(viewProjection, viewProjection);
    float edge = 0.0;
    // Zero projections and cancelled strand vectors have deterministic host
    // fallbacks. Source reciprocal-square-root behavior is undefined there.
    if (normalLength2 > 1e-12 && viewLength2 > 1e-12) {
        float cosine = clamp(dot(normalProjection, viewProjection)
            * inversesqrt(normalLength2 * viewLength2), 0.0, 1.0);
        edge = pow(cosine, edgeFadePower);
    }
    float projection = clamp(dot(primary, halfDirection), -1.0, 1.0);
    float u = clamp(sourceParameters.g
        * pow(characterProjectedHairSine(projection), 200.0), 0.0, 1.0);
    float exponent = trunc(max(1.0 - range2, 0.0) * 200.0);
    float lobe2 = pow(characterProjectedHairSine(
        clamp(dot(secondary, halfDirection), -1.0, 1.0)), exponent);
    return CharacterProjectedHairCoordinates(
        vec2(u, projection > 0.0 ? edge * edge : 0.0), edge, lobe2);
}

CharacterProjectedHairEnergy characterProjectedHairEnergy(
    CharacterProjectedHairCoordinates coordinates, vec3 rampSample,
    vec4 sourceParameters, float intensity, vec3 secondaryColor) {
    vec3 q = rampSample * coordinates.uv.x * coordinates.edgeFade;
    vec3 primary = q * sourceParameters.g * 0.04 * intensity * 5.0;
    vec3 secondary = sourceParameters.a * secondaryColor
        * coordinates.edgeFade * coordinates.secondaryLobe
        * (1.0 - max(max(q.r, q.g), q.b));
    return CharacterProjectedHairEnergy(q, primary, secondary);
}

vec2 characterProjectedHairLine(vec3 strand,vec3 normal,vec3 halfVector,float u,float lineSample,
    float parameterG,vec3 q,vec4 shape,vec2 color) {
    vec3 shifted=characterProjectedSafeNormalize(strand+normal*shape.y,vec3(1,0,0));
    float lobe=pow(characterProjectedHairSine(dot(shifted,halfVector)),trunc(max(1.0-shape.z,0.0)*200.0));
    float alternating=ceil(max(fract(u*shape.x)-.5,0.0));
    float weight=mix(alternating,1.0-lineSample,color.x);
    float base=mix(1.0,weight,shape.w);
    float factor=1.0+parameterG*(mix(base,1.0,max(max(q.r,q.g),q.b))-1.0)*lobe;
    return vec2(factor,mix(color.y,1.0,factor));
}

// Material-only terms. The caller supplies an explicit host light adapter.
// The source NPR globals and their runtime bindings are separate evidence.
struct CharacterEyeCoordinates {
    vec2 baseUv;
    vec3 lightingNormal;
    vec2 matcapUv;
    float whiteMask;
};

vec3 characterEyeSafeNormalize(vec3 value) {
    float length2=dot(value,value);
    return length2>1e-24?value*inversesqrt(length2):vec3(0,0,1);
}

CharacterEyeCoordinates characterEyeCoordinates(vec2 originalUv,vec3 view,
    vec3 normal,vec4 tangent,mat3 worldToView,float parallaxScale,float normalScale) {
    vec2 fracUv=fract(originalUv);
    float radius2=dot(fracUv-vec2(.5),fracUv-vec2(.5));
    float white=step(.25,radius2);
    float weight=clamp((.25-radius2)*5.0,0.0,1.0);
    weight=weight*weight*(3.0-2.0*weight);
    vec3 crossed=cross(normal,tangent.xyz);
    vec3 parallaxBitangent=crossed*(tangent.w>0.0?1.0:-1.0);
    float normalLength=max(length(normal),1e-12);
    vec3 projectedView=characterEyeSafeNormalize(vec3(dot(tangent.xyz,view),
        dot(parallaxBitangent,view),dot(normal,view))/normalLength);
    vec2 shifted=originalUv-projectedView.xy*vec2(1.0,.25)*parallaxScale*weight;
    vec2 radial=fracUv*2.0-1.0;
    vec3 matcapLocal=vec3(-radial*normalScale,sqrt(max(1.0-dot(radial,radial),0.0)));
    vec3 lightingLocal=mix(matcapLocal*vec3(-.125,-.125,1),vec3(0,0,1),white);
    vec3 bitangent=crossed*tangent.w;
    mat3 sourceFrame=mat3(tangent.xyz,bitangent,normal);
    vec3 lightNormal=characterEyeSafeNormalize(sourceFrame*lightingLocal);
    vec3 matcapNormal=characterEyeSafeNormalize(worldToView*sourceFrame*matcapLocal);
    return CharacterEyeCoordinates(shifted,lightNormal,matcapNormal.xy*.5+.5,white);
}

vec3 characterEyeDiffuseMultiplier(vec4 base,float white,vec3 highlight,vec3 scattering) {
    return mix(vec3(1),highlight,white)*mix(vec3(1),scattering,base.a);
}

float characterEyeAlphaMultiplier(float alpha,float premultiply) {
    return mix(1.0,alpha,premultiply);
}

vec3 characterEyeMatcap(vec4 sampleValue,vec4 color,float viewRampAlpha,
    float lightRampAlpha,float shadowSelect,float shadowFloor,vec3 illumination) {
    float ramp=mix(viewRampAlpha,min(lightRampAlpha,1.0),shadowSelect);
    float intensity=(.5*ramp+.5)*(ramp*(1.0-shadowFloor)+shadowFloor);
    return (sampleValue.rgb*color.a+sampleValue.a*color.rgb)*intensity*illumination;
}

vec3 characterEyeDirect(vec4 base,float white,vec3 highlight,vec3 scattering,
    vec3 weights,float premultiply) {
    return (base.rgb*weights.x+white*highlight*weights.y+base.a*scattering*weights.z)
        *characterEyeAlphaMultiplier(base.a,premultiply);
}
