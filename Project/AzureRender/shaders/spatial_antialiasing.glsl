#ifndef AZURE_SPATIAL_ANTIALIASING
#define AZURE_SPATIAL_ANTIALIASING
// Edge decisions use a perceptual approximation; color filtering stays linear.
float spatialLuma(vec3 color){return sqrt(max(dot(color,vec3(.299,.587,.114)),0.0));}
float spatialLumaAt(sampler2D source,vec2 uv){return spatialLuma(textureLod(source,uv,0).rgb);}
vec3 spatialEdgeResolve(sampler2D source,vec2 uv){
    vec2 pixel=1.0/vec2(textureSize(source,0));
    vec3 center=textureLod(source,uv,0).rgb;
    float m=spatialLuma(center);
    float n=spatialLumaAt(source,uv+vec2(0,-pixel.y));
    float s=spatialLumaAt(source,uv+vec2(0,pixel.y));
    float w=spatialLumaAt(source,uv+vec2(-pixel.x,0));
    float e=spatialLumaAt(source,uv+vec2(pixel.x,0));
    float low=min(m,min(min(n,s),min(w,e))),high=max(m,max(max(n,s),max(w,e)));
    float range=high-low;
    if(range<max(.0312,high*.125))return center;
    float nw=spatialLumaAt(source,uv-pixel);
    float ne=spatialLumaAt(source,uv+vec2(pixel.x,-pixel.y));
    float sw=spatialLumaAt(source,uv+vec2(-pixel.x,pixel.y));
    float se=spatialLumaAt(source,uv+pixel);
    float horizontal=abs(nw+sw-2*w)+2*abs(n+s-2*m)+abs(ne+se-2*e);
    float vertical=abs(nw+ne-2*n)+2*abs(w+e-2*m)+abs(sw+se-2*s);
    bool alongX=horizontal>=vertical;
    float negative=alongX?n:w,positive=alongX?s:e;
    float gradientNegative=abs(negative-m),gradientPositive=abs(positive-m);
    bool chooseNegative=gradientNegative>=gradientPositive;
    float gradient=max(gradientNegative,gradientPositive);
    float boundary=(m+(chooseNegative?negative:positive))*.5;
    vec2 across=alongX?vec2(0,pixel.y):vec2(pixel.x,0);
    if(chooseNegative)across=-across;
    vec2 tangent=alongX?vec2(pixel.x,0):vec2(0,pixel.y);
    vec2 start=uv+across*.5;
    vec2 a=start-tangent,b=start+tangent;
    float la=spatialLumaAt(source,a)-boundary,lb=spatialLumaAt(source,b)-boundary;
    bool foundA=abs(la)>=gradient*.25,foundB=abs(lb)>=gradient*.25;
    const float steps[10]=float[](1,1,1,1.5,2,2,2,4,8,8);
    for(int i=0;i<10;++i){
        if(foundA&&foundB)break;
        if(!foundA){a-=tangent*steps[i];la=spatialLumaAt(source,a)-boundary;foundA=abs(la)>=gradient*.25;}
        if(!foundB){b+=tangent*steps[i];lb=spatialLumaAt(source,b)-boundary;foundB=abs(lb)>=gradient*.25;}
    }
    float distanceA=alongX?uv.x-a.x:uv.y-a.y,distanceB=alongX?b.x-uv.x:b.y-uv.y;
    bool nearerA=distanceA<distanceB;
    float edgeOffset=.5-min(distanceA,distanceB)/max(distanceA+distanceB,1e-8);
    if(((nearerA?la:lb)<0)==((m-boundary)<0))edgeOffset=0;
    float average=((n+s+w+e)*2+nw+ne+sw+se)/12;
    float subpixel=clamp(abs(average-m)/range,0,1);
    subpixel=subpixel*subpixel*(3-2*subpixel);
    subpixel=subpixel*subpixel*.75;
    // A straight pixel-aligned boundary already has stable coverage. Apply
    // subpixel smoothing where neighboring rows/columns reveal a stair step.
    float tangentContrast=alongX?max(abs(nw-ne),abs(sw-se)):max(abs(nw-sw),abs(ne-se));
    subpixel*=clamp(tangentContrast/range,0,1);
    return textureLod(source,uv+across*max(edgeOffset,subpixel),0).rgb;
}
#endif
