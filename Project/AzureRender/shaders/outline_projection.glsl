#ifndef AZURE_OUTLINE_PROJECTION
#define AZURE_OUTLINE_PROJECTION 1

float outlineHalfFov(float projectionY) {
    float tangent=1.0/max(abs(projectionY),1e-8);
    float x=min(tangent,1.0/tangent);
    float squared=x*x;
    float polynomial=(.08729290217161179*squared-.30189499258995056)*squared+1.0;
    float angle=x*polynomial;
    return tangent<1.0?angle:1.5707961320877075-angle;
}

// The result is a clip-space offset, including the linear width mask.
// The view normal is normalized before applying the viewport aspect ratio.
vec2 outlineViewScaledOffset(float viewDepth,vec2 viewNormal,vec2 extent,
    float projectionY,float width,float minimumPixels,float widthMask) {
    float squared=dot(viewNormal,viewNormal);
    if(viewDepth<=0.0||width<=0.0||widthMask<=0.0||squared<=1e-12
        ||any(lessThanEqual(extent,vec2(0))))return vec2(0);
    float halfFov=outlineHalfFov(projectionY);
    vec2 direction=viewNormal*inversesqrt(squared)*vec2(extent.y/extent.x,1);
    vec2 offset=direction*(.3926990330219269/halfFov)*width
        *clamp(viewDepth*halfFov*4.583662509918213,0.0,1.0)*.004999999888241291;
    vec2 minimum=min(viewDepth,1.5707961320877075/halfFov)
        *(2.0*max(minimumPixels,0.0))/extent;
    return sign(offset)*max(abs(offset),minimum)*widthMask;
}

#endif
