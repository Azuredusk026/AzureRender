#ifndef AZURE_OUTLINE_PROJECTION
#define AZURE_OUTLINE_PROJECTION 1
#define AZURE_OUTLINE_DEPTH_PROJECTION 1
#define AZURE_OUTLINE_DIRECTION_ENCODING 1

vec3 outlineTangentDirection(vec2 uv,uint encoding) {
    if(encoding!=2u)return vec3(uv,sqrt(max(1.0-dot(uv,uv),0.0)));
    vec3 direction=vec3(uv,1.0-abs(uv.x)-abs(uv.y));
    if(direction.z<0.0)direction.xy=(1.0-abs(direction.yx))
        *vec2(direction.x>=0.0?1.0:-1.0,direction.y>=0.0?1.0:-1.0);
    return normalize(direction);
}

float outlineShiftedClipDepth(vec4 clip,vec2 depthSlopes,float shift) {
    if(shift==0.0||clip.w<=0.0)return clip.z;
    float shiftedW=clip.w+depthSlopes.y*shift;
    if(shiftedW<=1e-6)return -abs(clip.w);
    return (clip.z+depthSlopes.x*shift)*(clip.w/shiftedW);
}

float outlineHalfFov(float projectionY) {
    float tangent=1.0/max(abs(projectionY),1e-8);
    float x=min(tangent,1.0/tangent);
    float squared=x*x;
    float polynomial=(.08729290217161179*squared-.30189499258995056)*squared+1.0;
    float angle=x*polynomial;
    return tangent<1.0?angle:1.5707961320877075-angle;
}

// The result is a clip-space offset, including the linear width mask.
// The centered projected normal is normalized before the aspect correction.
vec2 outlineViewScaledOffset(float clipW,vec2 projectedNormal,vec2 extent,
    float projectionY,float width,float minimumPixels,float widthMask) {
    float squared=dot(projectedNormal,projectedNormal);
    if(clipW<=0.0||width<=0.0||widthMask<=0.0||squared<=1e-12
        ||any(lessThanEqual(extent,vec2(0))))return vec2(0);
    float halfFov=outlineHalfFov(projectionY);
    vec2 direction=projectedNormal*inversesqrt(squared)*vec2(extent.y/extent.x,1);
    vec2 offset=direction*(.3926990330219269/halfFov)*width
        *clamp(clipW*halfFov*4.583662509918213,0.0,1.0)*.004999999888241291;
    vec2 minimum=min(clipW,1.5707961320877075/halfFov)
        *(2.0*max(minimumPixels,0.0))/extent;
    return sign(offset)*max(abs(offset),minimum)*widthMask;
}

#endif
