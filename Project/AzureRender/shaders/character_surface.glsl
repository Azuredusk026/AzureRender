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
