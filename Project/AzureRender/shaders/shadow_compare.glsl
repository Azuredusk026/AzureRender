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
