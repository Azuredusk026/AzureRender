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
#ifdef AZURE_TEMPORAL_DISPLAY_MASK
layout(binding=7) uniform sampler2D currentDisplayMask;
layout(binding=8) uniform sampler2D historyDisplayMask;
layout(location=3) out vec2 outputDisplayHistory;
float reconstructedDisplayHistory(vec2 uv){
    vec2 position=uv*temporal.frame.xy-.5,base=floor(position),f=position-base;
    vec2 f2=f*f,f3=f2*f;
    vec2 w0=-.5*f+f2-.5*f3,w1=vec2(1)-2.5*f2+1.5*f3;
    vec2 w2=.5*f+2*f2-1.5*f3,w3=-.5*f2+.5*f3,w12=w1+w2;
    vec2 positions[3]=vec2[3]((base-.5)/temporal.frame.xy,
        (base+.5+w2/w12)/temporal.frame.xy,(base+2.5)/temporal.frame.xy);
    vec2 weights[3]=vec2[3](w0,w12,w3);
    float value=0;
    for(int y=0;y<3;++y)for(int x=0;x<3;++x)
        value+=textureLod(historyDisplayMask,vec2(positions[x].x,positions[y].y),0).r*weights[x].x*weights[y].y;
    return value;
}
#endif
// Subpixel velocities below this threshold share the stationary history sample.
const float stationaryPixelSpeed=.01;
bool cutoutMotion(float value){return value>=2&&value<3;}
bool finite4(vec4 value){return !any(isnan(value))&&!any(isinf(value));}
vec3 reconstructedHistory(vec2 uv){
    vec2 position=uv*temporal.frame.xy-.5,base=floor(position),f=position-base;
    vec2 f2=f*f,f3=f2*f;
    vec2 w0=-.5*f+f2-.5*f3,w1=vec2(1)-2.5*f2+1.5*f3;
    vec2 w2=.5*f+2*f2-1.5*f3,w3=-.5*f2+.5*f3,w12=w1+w2;
    vec2 positions[3]=vec2[3]((base-.5)/temporal.frame.xy,
        (base+.5+w2/w12)/temporal.frame.xy,(base+2.5)/temporal.frame.xy);
    vec2 weights[3]=vec2[3](w0,w12,w3);
    vec3 value=vec3(0);
    for(int y=0;y<3;++y)for(int x=0;x<3;++x)
        value+=textureLod(historyColor,vec2(positions[x].x,positions[y].y),0).rgb*weights[x].x*weights[y].y;
    return value;
}
float previousViewDepth(float deviceDepth){
    vec4 p=temporal.previousDepthProjection;float denominator=deviceDepth*p.z-p.x;
    return abs(denominator)>1e-12?abs((p.y-deviceDepth*p.w)/denominator):1e20;
}
void main(){
    ivec2 pixel=ivec2(gl_FragCoord.xy),extent=ivec2(temporal.frame.xy);
    vec2 uv=gl_FragCoord.xy/temporal.frame.xy;
    vec4 current=texelFetch(currentColor,pixel,0),motion=texelFetch(vectors,pixel,0);
#ifdef AZURE_TEMPORAL_DISPLAY_MASK
    float currentPolicy=texelFetch(currentDisplayMask,pixel,0).r;
    float basePolicyLow=currentPolicy,basePolicyHigh=currentPolicy,edgePolicyLow=currentPolicy,edgePolicyHigh=currentPolicy;
#endif
    bool projected=abs(motion.w)==4;
    if(projected)motion.w=sign(motion.w);
    bool reusable=finite4(current)&&finite4(motion)&&motion.w>-2;
    bool currentCutout=cutoutMotion(motion.w);
    bool responsive=motion.w<=-1||!finite4(motion);
    float depth=texelFetch(hardwareDepth,pixel,0).r;
    if(motion.w!=0)depth=texelFetch(surfaceDepth,pixel,0).r;
    vec3 baseLow=current.rgb,baseHigh=current.rgb,edgeLow=current.rgb,edgeHigh=current.rgb;
    float minimumDepth=depth,maximumDepth=depth;
    float innerMin=depth,innerMax=depth,innerDepth=depth;
    vec4 innerMotion=motion;
    // One bounded footprint supplies foreground motion, conservative depth
    // and colour bounds. It does not spatially average the current colour.
    for(int y=-2;y<=2;++y)for(int x=-2;x<=2;++x){
        ivec2 neighbour=clamp(pixel+ivec2(x,y),ivec2(0),extent-1);
        vec3 value=texelFetch(currentColor,neighbour,0).rgb;
        edgeLow=min(edgeLow,value);edgeHigh=max(edgeHigh,value);
        if(abs(x)<=1&&abs(y)<=1){baseLow=min(baseLow,value);baseHigh=max(baseHigh,value);}
#ifdef AZURE_TEMPORAL_DISPLAY_MASK
        float policy=texelFetch(currentDisplayMask,neighbour,0).r;
        edgePolicyLow=min(edgePolicyLow,policy);edgePolicyHigh=max(edgePolicyHigh,policy);
        if(abs(x)<=1&&abs(y)<=1){basePolicyLow=min(basePolicyLow,policy);basePolicyHigh=max(basePolicyHigh,policy);}
#endif
        vec4 candidate=texelFetch(vectors,neighbour,0);
        if(abs(candidate.w)==4){
            projected=true;
            candidate.w=sign(candidate.w);
        }
        float candidateDepth=candidate.w!=0?texelFetch(surfaceDepth,neighbour,0).r:texelFetch(hardwareDepth,neighbour,0).r;
        minimumDepth=min(minimumDepth,candidateDepth);maximumDepth=max(maximumDepth,candidateDepth);
        bool inner=abs(x)<=1&&abs(y)<=1;
        if(inner){innerMin=min(innerMin,candidateDepth);innerMax=max(innerMax,candidateDepth);}
        if((inner||cutoutMotion(candidate.w))&&candidateDepth<innerDepth){
            innerDepth=candidateDepth;innerMotion=candidate;
        }
        bool stationary=length(candidate.xy*temporal.frame.xy)<=stationaryPixelSpeed;
        if((inner||stationary||cutoutMotion(candidate.w))&&candidateDepth<depth){depth=candidateDepth;motion=candidate;}
    }
    if(!projected){
        minimumDepth=innerMin;maximumDepth=innerMax;
        depth=innerDepth;motion=innerMotion;
    }
    vec2 previousUv=uv-motion.xy;float expectedDepth=motion.z;
    bool valid=temporal.frame.z>.5&&!responsive&&motion.w>-1&&finite4(current)&&finite4(motion);
    if(valid&&motion.w==0){
        vec4 world=temporal.inverseCurrent*vec4(uv*2-1,depth,1),previous=temporal.previous*world;
        valid=finite4(previous)&&previous.w>1e-8;
        if(valid){previousUv=previous.xy/previous.w*.5+.5;expectedDepth=previous.z/previous.w;}
    }
    valid=valid&&all(greaterThanEqual(previousUv,vec2(0)))&&all(lessThanEqual(previousUv,vec2(1)))&&expectedDepth>=0&&expectedDepth<=1;
    float nearestViewDepth=previousViewDepth(minimumDepth);
    float baseContrast=max(max(baseHigh.r-baseLow.r,baseHigh.g-baseLow.g),baseHigh.b-baseLow.b);
    bool coverageEdge=baseContrast>.001*max(1,max(max(baseHigh.r,baseHigh.g),baseHigh.b))&&previousViewDepth(maximumDepth)-nearestViewDepth>temporal.limits.x+.05*nearestViewDepth;
    coverageEdge=coverageEdge||(!currentCutout&&cutoutMotion(motion.w));
    vec3 low=coverageEdge?edgeLow:baseLow,high=coverageEdge?edgeHigh:baseHigh;
    vec3 accumulated=current.rgb;
#ifdef AZURE_TEMPORAL_DISPLAY_MASK
    float policyLow=coverageEdge?edgePolicyLow:basePolicyLow,policyHigh=coverageEdge?edgePolicyHigh:basePolicyHigh;
    float accumulatedPolicy=currentPolicy;
#endif
    float speed=length(motion.xy*temporal.frame.xy);
    if(valid){
        ivec2 center=clamp(ivec2(previousUv*temporal.frame.xy),ivec2(0),extent-1);
        float expectedViewDepth=previousViewDepth(expectedDepth),sampledViewDepth=previousViewDepth(texelFetch(historyDepth,center,0).r);
        float tolerance=temporal.limits.x+temporal.limits.y*expectedViewDepth;
        vec4 history=texture(historyColor,previousUv);
        if(length((uv-previousUv)*temporal.frame.xy)>stationaryPixelSpeed)history.rgb=reconstructedHistory(previousUv);
        valid=abs(expectedViewDepth-sampledViewDepth)<=tolerance&&finite4(history)&&history.a>.5;
#ifdef AZURE_TEMPORAL_DISPLAY_MASK
        float historyPolicy=texture(historyDisplayMask,previousUv).r;
        if(length((uv-previousUv)*temporal.frame.xy)>stationaryPixelSpeed)historyPolicy=reconstructedDisplayHistory(previousUv);
        valid=valid&&!isnan(historyPolicy)&&!isinf(historyPolicy);
#endif
        if(valid){float weight=mix(temporal.limits.z,.8,clamp(speed/8,0,1));
            if(!coverageEdge)weight*=1-(cutoutMotion(motion.w)?motion.w-2:clamp(-motion.w,0,1));
            float contrast=max(max(high.r-low.r,high.g-low.g),high.b-low.b);
            bool preserveCoverage=cutoutMotion(motion.w)||(projected&&speed<=stationaryPixelSpeed);
            float clipping=coverageEdge&&preserveCoverage&&contrast>1e-5?mix(temporal.limits.w,1,clamp(speed,0,1)):1;
            vec3 bounded=mix(history.rgb,clamp(history.rgb,low,high),clipping);
            accumulated=mix(current.rgb,bounded,weight);
#ifdef AZURE_TEMPORAL_DISPLAY_MASK
            float boundedPolicy=mix(historyPolicy,clamp(historyPolicy,policyLow,policyHigh),clipping);
            accumulatedPolicy=mix(currentPolicy,boundedPolicy,weight);
#endif
        }
    }
    // Display reconstruction is bounded by current geometry colours.
    // Preserve the accumulated history to keep sharpening out of feedback.
    vec3 displayed=accumulated;
#ifdef AZURE_TEMPORAL_DISPLAY_MASK
    float displayedPolicy=accumulatedPolicy;
#endif
    if(valid&&coverageEdge&&!currentCutout&&!responsive&&motion.w==1&&(!projected||speed>stationaryPixelSpeed)){
        vec3 midpoint=(low+high)*.5;
        float gain=1+.5*clamp(speed*4,0,1);
        displayed=clamp(midpoint+(accumulated-midpoint)*gain,low,high);
#ifdef AZURE_TEMPORAL_DISPLAY_MASK
        float policyMidpoint=(policyLow+policyHigh)*.5;
        displayedPolicy=clamp(policyMidpoint+(accumulatedPolicy-policyMidpoint)*gain,policyLow,policyHigh);
#endif
    }
    outputColor=vec4(displayed,current.a);outputHistory=vec4(accumulated,reusable?1:0);outputDepth=depth;
#ifdef AZURE_TEMPORAL_DISPLAY_MASK
    outputDisplayHistory=clamp(vec2(accumulatedPolicy,displayedPolicy),0,1);
#endif
}
