float dielectricF0(float specular) {
    // UE's authored dielectric specular uses 0.5 for the usual 4% reflectance.
    return 0.08 * clamp(specular, 0.0, 1.0);
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
