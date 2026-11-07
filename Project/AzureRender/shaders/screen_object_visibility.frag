#version 450
struct ObjectRecord {mat4 projection;vec4 lightBias;vec4 normalFilter;vec4 rect;};
layout(binding=0,std140) uniform Parameters {
    mat4 clipToWorld;vec4 reconstruction;uvec4 control;ObjectRecord records[8];
} frame;
layout(binding=1) uniform sampler2D deviceDepth;
layout(binding=2) uniform sampler2D octNormals;
layout(binding=3) uniform usampler2D objectIds;
layout(binding=4) uniform sampler2D objectAtlas;
layout(location=0) out vec2 visibility;
const uvec4 disk[16]=uvec4[16](
    uvec4(3201081267u,1063955090u,3211864058u,3201061344u),uvec4(1040121934u,3212706305u,1064440302u,3208959771u),
    uvec4(1062876132u,1057349892u,3183535000u,3211652203u),uvec4(3194671333u,1064906152u,1051762308u,1050048288u),
    uvec4(3209018472u,1059231696u,3211425662u,1055545736u),uvec4(1061876180u,3206286094u,3209740500u,3210808912u),
    uvec4(3205757713u,1062266235u,3200514852u,1049474200u),uvec4(3210003689u,3205389102u,1064931168u,1061267692u),
    uvec4(1062128548u,1058465561u,1055059832u,3212419372u),uvec4(1056005468u,1063372016u,1057592576u,3203566922u),
    uvec4(3198206253u,3211988469u,3196561928u,3201728006u),uvec4(3211914230u,3198645051u,1061863138u,1044609992u),
    uvec4(3191700082u,3212547147u,3195515288u,1065303976u),uvec4(3211725472u,1052196876u,3209717972u,1063916682u),
    uvec4(3212796835u,1032676071u,1045209904u,1061769832u),uvec4(1050459336u,1064547749u,1041451160u,3188745312u));
bool finite3(vec3 v){return !any(isnan(v))&&!any(isinf(v));}
float reversedTexel(ivec2 p,ivec2 lo,ivec2 hi){
    if(any(lessThan(p,lo))||any(greaterThanEqual(p,hi)))return 0;
    float depth=texelFetch(objectAtlas,p,0).r;return frame.control.z!=0u?1-depth:depth;
}
vec4 reversedGather(vec2 uv,ivec2 lo,ivec2 hi){
    ivec2 p=ivec2(floor(uv*vec2(textureSize(objectAtlas,0))-.5));
    if(all(greaterThanEqual(p,lo))&&all(lessThan(p+1,hi))){
        vec4 depth=textureGather(objectAtlas,uv);return frame.control.z!=0u?1-depth:depth;
    }
    return vec4(reversedTexel(p+ivec2(0,1),lo,hi),reversedTexel(p+ivec2(1),lo,hi),reversedTexel(p+ivec2(1,0),lo,hi),reversedTexel(p,lo,hi));
}
void main(){
    visibility=vec2(1);ivec2 pixel=ivec2(gl_FragCoord.xy);uint identity=texelFetch(objectIds,pixel,0).r;
    if(identity==0u||identity>frame.control.x)return;
    float depth=texelFetch(deviceDepth,pixel,0).r;if(isnan(depth)||isinf(depth)||depth<0||depth>1)return;
    vec2 ndc=fma((vec2(pixel)+frame.reconstruction.zw)*frame.reconstruction.xy,vec2(2),vec2(-1));
    vec4 h=fma(frame.clipToWorld[0],vec4(ndc.x),(frame.control.y!=0u?-ndc.y:ndc.y)*frame.clipToWorld[1]);
    h=fma(frame.clipToWorld[2],vec4(depth),h);h+=frame.clipToWorld[3];
    if(isnan(h.w)||isinf(h.w)||abs(h.w)<1e-20)return;vec3 world=h.xyz/h.w;if(!finite3(world))return;
    vec2 oct=fma(texelFetch(octNormals,pixel,0).xy,vec2(2),vec2(-1));vec3 normal=vec3(oct.x,1-dot(abs(oct),vec2(1)),oct.y);
    if(normal.y<0)normal.xz=(1-abs(oct.yx))*mix(vec2(-1),vec2(1),greaterThanEqual(oct,vec2(0)));
    normal*=inversesqrt(dot(normal,normal));
    ObjectRecord record=frame.records[identity-1];vec3 light=record.lightBias.xyz;float weight=1-clamp(dot(normal,light),0,.9);
    vec2 bias=weight*vec2(record.lightBias.w,record.normalFilter.x);
    vec3 biased=fma(-light,vec3(bias.x),world);biased=fma(normal,vec3(bias.y),biased);
    vec3 q=fma(record.projection[0].xyz,vec3(biased.x),biased.y*record.projection[1].xyz);
    q=fma(record.projection[2].xyz,vec3(biased.z),q);q+=record.projection[3].xyz;
    if(!finite3(q)||any(lessThanEqual(q.xy,vec2(0)))||any(greaterThanEqual(q.xy,vec2(1))))return;
    if(frame.control.z!=0u)q.z=1-q.z;q.z=max(q.z,record.normalFilter.w);if(q.z>=1)return;
    vec2 uv=fma(q.xy,record.rect.zw,record.rect.xy);vec2 size=vec2(textureSize(objectAtlas,0));
    ivec2 lo=ivec2(ceil(record.rect.xy*size-.5)),hi=ivec2(ceil((record.rect.xy+record.rect.zw)*size-.5));
    uint phaseY=frame.control.w!=0u?uint(textureSize(objectIds,0).y-1-pixel.y):uint(pixel.y);
    uint phase=((uint(pixel.x)&3u)<<2)|(phaseY&3u);vec2 rotation=uintBitsToFloat(disk[phase]).xy;
    float count=0,delta=0;
    for(uint i=0;i<16;++i){
        vec2 offset=uintBitsToFloat(disk[i]).zw;vec2 rotated=vec2(dot(offset,vec2(rotation.x,-rotation.y)),dot(offset.yx,rotation));
        vec2 tap=fma(rotated,record.normalFilter.yz,uv);vec4 difference=reversedGather(tap,lo,hi)-q.z;
        vec4 included=vec4(greaterThanEqual(difference,vec4(0)));count+=dot(included,vec4(1));delta+=dot(difference,included);
    }
    if(count>0){
        float x=fma(count,.03125,-1),signX=sign(x),t=fma(-signX,x,1),t2=t*t,t3=t*t2;
        float ratio=clamp((1/q.z)*((1/count)*delta),0,1),span=fma(-t2,t,t),adjusted=fma(ratio,span,t3);
        visibility.y=min(fma(-signX*(1-adjusted),.5,.5),1);
    }
}
