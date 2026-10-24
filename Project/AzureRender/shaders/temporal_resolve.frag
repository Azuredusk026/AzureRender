#version 450
layout(binding=0) uniform sampler2D currentColor;
layout(binding=1) uniform sampler2D historyColor;
layout(binding=2) uniform sampler2D historyDepth;
layout(binding=3) uniform sampler2D hardwareDepth;
layout(binding=4) uniform sampler2D vectors;
layout(binding=5) uniform sampler2D surfaceDepth;
layout(std140,binding=6) uniform TemporalData {vec4 frame;mat4 inverseCurrent;mat4 previous;vec4 limits;vec4 previousDepthProjection;} temporal;
layout(location=0) out vec4 outputColor;
layout(location=1) out vec4 outputHistory;
layout(location=2) out float outputDepth;
bool finite4(vec4 value){return !any(isnan(value))&&!any(isinf(value));}
float previousViewDepth(float deviceDepth){
    vec4 p=temporal.previousDepthProjection;float denominator=deviceDepth*p.z-p.x;
    return abs(denominator)>1e-12?abs((p.y-deviceDepth*p.w)/denominator):1e20;
}
void main(){
    ivec2 pixel=ivec2(gl_FragCoord.xy),extent=ivec2(temporal.frame.xy);
    vec2 uv=gl_FragCoord.xy/temporal.frame.xy;
    vec4 current=texelFetch(currentColor,pixel,0),motion=texelFetch(vectors,pixel,0);
    bool responsive=motion.w<0||!finite4(motion);
    float depth=texelFetch(hardwareDepth,pixel,0).r;
    if(motion.w!=0)depth=texelFetch(surfaceDepth,pixel,0).r;
    vec3 baseLow=current.rgb,baseHigh=current.rgb,edgeLow=current.rgb,edgeHigh=current.rgb;
    float minimumDepth=depth,maximumDepth=depth;
    // One bounded footprint supplies foreground motion, conservative depth
    // and colour bounds. It does not spatially average the current colour.
    for(int y=-2;y<=2;++y)for(int x=-2;x<=2;++x){
        ivec2 neighbour=clamp(pixel+ivec2(x,y),ivec2(0),extent-1);
        vec3 value=texelFetch(currentColor,neighbour,0).rgb;
        edgeLow=min(edgeLow,value);edgeHigh=max(edgeHigh,value);
        if(abs(x)<=1&&abs(y)<=1){baseLow=min(baseLow,value);baseHigh=max(baseHigh,value);}
        vec4 candidate=texelFetch(vectors,neighbour,0);
        float candidateDepth=candidate.w!=0?texelFetch(surfaceDepth,neighbour,0).r:texelFetch(hardwareDepth,neighbour,0).r;
        if(abs(x)<=1&&abs(y)<=1){minimumDepth=min(minimumDepth,candidateDepth);maximumDepth=max(maximumDepth,candidateDepth);}
        if(candidateDepth<depth){depth=candidateDepth;motion=candidate;}
    }
    vec2 previousUv=uv-motion.xy;float expectedDepth=motion.z;
    bool valid=temporal.frame.z>.5&&!responsive&&motion.w>=0&&finite4(current)&&finite4(motion);
    if(valid&&motion.w==0){
        vec4 world=temporal.inverseCurrent*vec4(uv*2-1,depth,1),previous=temporal.previous*world;
        valid=finite4(previous)&&previous.w>1e-8;
        if(valid){previousUv=previous.xy/previous.w*.5+.5;expectedDepth=previous.z/previous.w;}
    }
    valid=valid&&all(greaterThanEqual(previousUv,vec2(0)))&&all(lessThanEqual(previousUv,vec2(1)))&&expectedDepth>=0&&expectedDepth<=1;
    float nearestViewDepth=previousViewDepth(minimumDepth);
    float baseContrast=max(max(baseHigh.r-baseLow.r,baseHigh.g-baseLow.g),baseHigh.b-baseLow.b);
    bool coverageEdge=baseContrast>.001*max(1,max(max(baseHigh.r,baseHigh.g),baseHigh.b))&&previousViewDepth(maximumDepth)-nearestViewDepth>temporal.limits.x+.05*nearestViewDepth;
    vec3 low=coverageEdge?edgeLow:baseLow,high=coverageEdge?edgeHigh:baseHigh;
    vec3 accumulated=current.rgb;
    if(valid){
        ivec2 center=clamp(ivec2(previousUv*temporal.frame.xy),ivec2(0),extent-1);
        float expectedViewDepth=previousViewDepth(expectedDepth),sampledViewDepth=previousViewDepth(texelFetch(historyDepth,center,0).r);
        float tolerance=temporal.limits.x+temporal.limits.y*expectedViewDepth;
        vec4 history=texture(historyColor,previousUv);
        valid=abs(expectedViewDepth-sampledViewDepth)<=tolerance&&finite4(history)&&history.a>.5;
        if(valid){float speed=length(motion.xy*temporal.frame.xy);float weight=mix(temporal.limits.z,.8,clamp(speed/8,0,1));
            float contrast=max(max(high.r-low.r,high.g-low.g),high.b-low.b);
            float clipping=coverageEdge&&contrast>1e-5?mix(temporal.limits.w,1,clamp(speed,0,1)):1;
            vec3 bounded=mix(history.rgb,clamp(history.rgb,low,high),clipping);
            accumulated=mix(current.rgb,bounded,weight);}
    }
    outputColor=vec4(accumulated,current.a);outputHistory=vec4(accumulated,1);outputDepth=depth;
}
