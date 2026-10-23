#version 450
layout(binding=0) uniform sampler2D currentColor;
layout(binding=1) uniform sampler2D historyColor;
layout(binding=2) uniform sampler2D historyDepth;
layout(binding=3) uniform sampler2D hardwareDepth;
layout(binding=4) uniform sampler2D vectors;
layout(binding=5) uniform sampler2D surfaceDepth;
layout(std140,binding=6) uniform TemporalData {vec4 frame;mat4 inverseCurrent;mat4 previous;vec4 limits;} temporal;
layout(location=0) out vec4 outputColor;
layout(location=1) out vec4 outputHistory;
layout(location=2) out float outputDepth;
bool finite4(vec4 value){return !any(isnan(value))&&!any(isinf(value));}
void main(){
    ivec2 pixel=ivec2(gl_FragCoord.xy),extent=ivec2(temporal.frame.xy);
    vec2 uv=gl_FragCoord.xy/temporal.frame.xy;
    vec4 current=texelFetch(currentColor,pixel,0),motion=texelFetch(vectors,pixel,0);
    float depth=texelFetch(hardwareDepth,pixel,0).r;
    if(motion.w!=0)depth=texelFetch(surfaceDepth,pixel,0).r;
    vec2 previousUv=uv-motion.xy;float expectedDepth=motion.z;
    bool valid=temporal.frame.z>.5&&motion.w>=0&&finite4(current)&&finite4(motion);
    if(valid&&motion.w==0){
        vec4 world=temporal.inverseCurrent*vec4(uv*2-1,depth,1),previous=temporal.previous*world;
        valid=finite4(previous)&&previous.w>1e-8;
        if(valid){previousUv=previous.xy/previous.w*.5+.5;expectedDepth=previous.z/previous.w;}
    }
    valid=valid&&all(greaterThanEqual(previousUv,vec2(0)))&&all(lessThanEqual(previousUv,vec2(1)))&&expectedDepth>=0&&expectedDepth<=1;
    vec3 low=current.rgb,high=current.rgb;
    for(int y=-1;y<=1;++y)for(int x=-1;x<=1;++x){vec3 value=texelFetch(currentColor,clamp(pixel+ivec2(x,y),ivec2(0),extent-1),0).rgb;low=min(low,value);high=max(high,value);}
    vec3 accumulated=current.rgb;
    if(valid){
        ivec2 center=clamp(ivec2(previousUv*temporal.frame.xy),ivec2(0),extent-1);
        float minimum=1,maximum=0;
        for(int y=-1;y<=1;++y)for(int x=-1;x<=1;++x){float z=texelFetch(historyDepth,clamp(center+ivec2(x,y),ivec2(0),extent-1),0).r;minimum=min(minimum,z);maximum=max(maximum,z);}
        float tolerance=temporal.limits.x+temporal.limits.y*abs(expectedDepth);
        vec4 history=texture(historyColor,previousUv);
        valid=expectedDepth>=minimum-tolerance&&expectedDepth<=maximum+tolerance&&finite4(history)&&history.a>.5;
        if(valid){float speed=length(motion.xy*temporal.frame.xy);float weight=mix(temporal.limits.z,.8,clamp(speed/8,0,1));
            accumulated=mix(current.rgb,clamp(history.rgb,low,high),weight);}
    }
    outputColor=vec4(accumulated,current.a);outputHistory=vec4(accumulated,1);outputDepth=depth;
}
