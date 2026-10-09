float compareShadowDepths(vec4 depths, float receiver, vec2 weight) {
    vec4 visible=step(vec4(receiver),depths);
    return mix(mix(visible.x,visible.y,weight.x),mix(visible.z,visible.w,weight.x),weight.y);
}
float compareShadowDepths(vec4 depths,float receiver,vec2 weight,vec2 depthPerTexel) {
    vec4 plane=receiver+vec4(
        dot(vec2(0,0)-weight,depthPerTexel),
        dot(vec2(1,0)-weight,depthPerTexel),
        dot(vec2(0,1)-weight,depthPerTexel),
        dot(vec2(1,1)-weight,depthPerTexel));
    vec4 visible=step(plane,depths);
    return mix(mix(visible.x,visible.y,weight.x),mix(visible.z,visible.w,weight.x),weight.y);
}

vec2 shadowBlockerMoments(vec4 depths, float receiver, vec2 weight) {
    vec4 covered = vec4(1.0)-step(vec4(receiver),depths);
    vec4 footprint = vec4((1.0-weight.x)*(1.0-weight.y), weight.x*(1.0-weight.y),
                         (1.0-weight.x)*weight.y, weight.x*weight.y);
    vec4 mass = covered*footprint;
    return vec2(dot(depths,mass),dot(vec4(1.0),mass));
}

float shadowDiskTexelWeight(vec2 offset, float radius) {
    // A one-texel boundary ramp approximates the area covered by the disk.
    // Every covered texel contributes once; narrow blockers cannot fall
    // between a sparse set of taps or acquire a moving spiral pattern.
    return clamp(radius + 0.5 - length(offset), 0.0, 1.0);
}
float shadowReceiverBias(float normalDotLight, float worldTexel) {
    float sine = sqrt(max(1.0-normalDotLight*normalDotLight, 0.0));
    return max(max(.003*(1.0-normalDotLight), .001), 2.0*worldTexel*sine);
}
float shadowReceiverPlaneWeight(float normalCurvature, float filterWorldRadius) {
    // Receiver-plane derivatives describe one triangle. Smooth vertex normals
    // reveal curvature; extending that triangle across a curved footprint
    // creates false, triangle-shaped blockers on the neighbouring surface.
    return 1.0-smoothstep(.01,.05,normalCurvature*filterWorldRadius);
}

float shadowContactRadius(float tileTexels, float maximumRadius) {
    // A three-texel disk at the reference density preserves the effective
    // softness of the square contact footprint while resolving every texel.
    return min(3.0 * tileTexels / 1024.0, maximumRadius);
}

vec4 shadowTexelQuad(sampler2D depthMap, ivec2 base, ivec2 lo, ivec2 hi) {
    if(all(greaterThanEqual(base,lo)) && all(lessThanEqual(base+ivec2(1),hi))) {
        vec2 uv=(vec2(base)+1.0)/vec2(textureSize(depthMap,0));
        return textureGather(depthMap,uv).wzxy;
    }
    return vec4(texelFetch(depthMap,clamp(base,lo,hi),0).r,
                texelFetch(depthMap,clamp(base+ivec2(1,0),lo,hi),0).r,
                texelFetch(depthMap,clamp(base+ivec2(0,1),lo,hi),0).r,
                texelFetch(depthMap,clamp(base+ivec2(1),lo,hi),0).r);
}

vec4 shadowDiskQuadWeights(vec2 offset, float radius) {
    vec4 x=offset.x+vec4(0,1,0,1),y=offset.y+vec4(0,0,1,1);
    return clamp(vec4(radius+.5)-sqrt(x*x+y*y),vec4(0),vec4(1));
}
