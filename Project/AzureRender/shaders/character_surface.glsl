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
    vec3 axis = characterProjectedSafeNormalize(
        objectX * directionX + objectY, vec3(0, 1, 0));
    vec3 strand = cross(normal,
        mix(cross(normal, axis), tangent.xyz, sourceParameters.r))
        * mix(1.0, tangent.w, sourceParameters.r);
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
