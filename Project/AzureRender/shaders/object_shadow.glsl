#ifndef AZURE_OBJECT_SHADOW_GLSL
#define AZURE_OBJECT_SHADOW_GLSL
struct ObjectShadowRecord {
    mat4 worldToUnit;
    vec4 biasAndFilter;
    vec4 atlasRect;
    vec4 receiverSampling;
};
// The caller supplies pairs from the shared depth-bounds/record buffer.
vec2 objectShadowRecordPair(uint index);
bool objectShadowLoadRecord(uint depthPairCount,uint index,out ObjectShadowRecord record) {
    if(index==0u)return false;
    uint start=(depthPairCount+1u)&~1u;
    uvec4 header=floatBitsToUint(vec4(objectShadowRecordPair(start),objectShadowRecordPair(start+1u)));
    if(header.x!=2u || header.y>header.z || header.z>8u || index>header.y)return false;
    start+=2u+(index-1u)*14u;
    vec4 rows[7];
    for(uint i=0u;i<7u;++i)rows[i]=vec4(objectShadowRecordPair(start+i*2u),objectShadowRecordPair(start+i*2u+1u));
    record.worldToUnit=mat4(rows[0],rows[1],rows[2],rows[3]);
    record.biasAndFilter=rows[4];record.atlasRect=rows[5];record.receiverSampling=rows[6];return true;
}
// Top-left pixel offsets move the receiver at constant view depth. Camera basis
// and projection support both perspective and orthographic, including lens shift.
vec3 objectShadowReceiverPosition(vec3 position,vec2 offset,mat4 worldToClip,
    vec3 cameraRight,vec3 cameraUp,vec2 extent) {
    float w=(worldToClip*vec4(position,1)).w;
    vec2 scale=vec2((worldToClip*vec4(cameraRight,0)).x,(worldToClip*vec4(cameraUp,0)).y)*extent;
    if(w<=0.0||any(lessThan(abs(scale),vec2(1e-6))))return position;
    vec2 shift=2.0*w*offset/scale;
    return position+cameraRight*shift.x+cameraUp*shift.y;
}
vec3 objectShadowReceiver(ObjectShadowRecord record,vec3 position,vec3 normal) {
    vec3 light=normalize(vec3(record.worldToUnit[0].z,record.worldToUnit[1].z,record.worldToUnit[2].z));
    float weight=1.0-clamp(dot(normal,light),0.0,.9);
    vec3 biased=position-light*record.biasAndFilter.y*weight+normal*record.biasAndFilter.x*weight;
    vec3 projected=(record.worldToUnit*vec4(biased,1)).xyz;
    return vec3(projected.xy,max(1.0-projected.z,.01));
}
// Fixed 16-tap distribution; XY rotates the disk and ZW holds its offset.
const uvec4 objectShadowDiskBits[16]=uvec4[16](
    uvec4(3201081267u,1063955090u,3211864058u,3201061344u),
    uvec4(1040121934u,3212706305u,1064440302u,3208959771u),
    uvec4(1062876132u,1057349892u,3183535000u,3211652203u),
    uvec4(3194671333u,1064906152u,1051762308u,1050048288u),
    uvec4(3209018472u,1059231696u,3211425662u,1055545736u),
    uvec4(1061876180u,3206286094u,3209740500u,3210808912u),
    uvec4(3205757713u,1062266235u,3200514852u,1049474200u),
    uvec4(3210003689u,3205389102u,1064931168u,1061267692u),
    uvec4(1062128548u,1058465561u,1055059832u,3212419372u),
    uvec4(1056005468u,1063372016u,1057592576u,3203566922u),
    uvec4(3198206253u,3211988469u,3196561928u,3201728006u),
    uvec4(3211914230u,3198645051u,1061863138u,1044609992u),
    uvec4(3191700082u,3212547147u,3195515288u,1065303976u),
    uvec4(3211725472u,1052196876u,3209717972u,1063916682u),
    uvec4(3212796835u,1032676071u,1045209904u,1061769832u),
    uvec4(1050459336u,1064547749u,1041451160u,3188745312u));
float objectShadowDistribution(float count,float positiveDelta,float receiver) {
    if(count==0.0)return 1.0;
    float x=2.0*(count/64.0)-1.0;
    float q=1.0-abs(x),cube=q*q*q;
    float ratio=clamp(positiveDelta/count/receiver,0.0,1.0);
    return .5-.5*sign(x)*(1.0-(cube+ratio*(q-cube)));
}
float objectShadowUnorm8(float visibility) { return floor(clamp(visibility,0.0,1.0)*255.0+.5)/255.0; }
float objectShadowPageTexel(texture2D depth,sampler sharedSampler,ivec2 pixel,ivec2 lo,ivec2 hi) {
    if(any(lessThan(pixel,lo))||any(greaterThanEqual(pixel,hi)))return 1.0;
    return texelFetch(sampler2D(depth,sharedSampler),pixel,0).r;
}
vec4 objectShadowGather(texture2D depth,sampler sharedSampler,vec2 uv,ivec2 lo,ivec2 hi) {
    ivec2 base=ivec2(floor(uv*vec2(textureSize(sampler2D(depth,sharedSampler),0))-.5));
    if(all(greaterThanEqual(base,lo))&&all(lessThan(base+1,hi)))
        return textureGather(sampler2D(depth,sharedSampler),uv);
    return vec4(objectShadowPageTexel(depth,sharedSampler,base+ivec2(0,1),lo,hi),
                objectShadowPageTexel(depth,sharedSampler,base+ivec2(1),lo,hi),
                objectShadowPageTexel(depth,sharedSampler,base+ivec2(1,0),lo,hi),
                objectShadowPageTexel(depth,sharedSampler,base,lo,hi));
}
// gridPixel uses a bottom-left origin; UV refers to the complete forward-depth atlas.
vec2 objectShadowFilter(texture2D depth,sampler sharedSampler,vec2 uv,float receiver,
    ivec2 gridPixel,vec4 rect,vec2 radius) {
    ivec2 extent=textureSize(sampler2D(depth,sharedSampler),0);
    ivec2 lo=ivec2(round(rect.xy*vec2(extent))),hi=ivec2(round((rect.xy+rect.zw)*vec2(extent)));
    uint phase=uint(((gridPixel.x&3)<<2)|(gridPixel.y&3));
    vec2 rotation=uintBitsToFloat(objectShadowDiskBits[phase]).xy;
    float count=0.0,sumDelta=0.0;
    for(int i=0;i<16;++i){
        vec2 offset=uintBitsToFloat(objectShadowDiskBits[i]).zw;
        precise vec2 rotated=vec2(offset.x*rotation.x-offset.y*rotation.y,offset.y*rotation.x+offset.x*rotation.y);
        precise vec2 tapUv=uv+rotated*(radius/vec2(extent));
        vec4 reversed=1.0-objectShadowGather(depth,sharedSampler,tapUv,lo,hi);
        count+=dot(step(vec4(receiver),reversed),vec4(1));
        sumDelta+=dot(max(reversed-receiver,vec4(0)),vec4(1));
    }
    return vec2(objectShadowDistribution(count,sumDelta,receiver),count);
}
float objectShadowVisibility(texture2D depth,sampler sharedSampler,ObjectShadowRecord record,
    vec3 position,vec3 normal,ivec2 gridPixel) {
    vec3 receiver=objectShadowReceiver(record,position,normal);
    if(any(lessThanEqual(receiver.xy,vec2(0)))||any(greaterThanEqual(receiver.xy,vec2(1)))||receiver.z>=1.0)return 1.0;
    vec2 uv=record.atlasRect.xy+receiver.xy*record.atlasRect.zw;
    return objectShadowUnorm8(objectShadowFilter(depth,sharedSampler,uv,receiver.z,gridPixel,record.atlasRect,record.biasAndFilter.zw).x);
}
#endif
