#version 450

layout(binding = 0) uniform sampler2D normalTexture;
layout(binding = 1) uniform sampler2D depthTexture;
layout(binding = 2) uniform sampler2D shadowTexture;
layout(binding = 3) uniform sampler2D sceneColorTexture;

layout(push_constant) uniform OutlineParameters {
    float strength;
    float depthThreshold;
    float normalThreshold;
    float diagnosticView;
    float exposureEv;
    float toneMappingEnabled;
    float bloomStrength;
    float bloomIsolation;
    vec4 outlineColor;
    vec4 gradeParameters;
    vec4 gradeTint;
    vec4 sampling;
} outline;

layout(location = 0) in vec2 screenUv;
layout(location = 0) out vec4 outputColor;

vec3 acesFitted(vec3 color) {
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp(
        (color * (a * color + b))
            / (color * (c * color + d) + e),
        0.0,
        1.0);
}

float linearDepth(float z) {
    return outline.sampling.y*outline.sampling.z / max(outline.sampling.z-z*(outline.sampling.z-outline.sampling.y),.00001);
}
vec4 shadePixel(vec2 screenUv) {
    ivec2 textureExtent = textureSize(normalTexture, 0);
    vec2 texelSize = 1.0 / vec2(textureExtent);
    vec4 centerSample = texture(normalTexture, screenUv);
    int diagnosticView = int(floor(outline.diagnosticView + 0.5));
    if (diagnosticView == 1) {
        vec3 normalColor = centerSample.a < 0.01
            ? vec3(0.018, 0.024, 0.040)
            : centerSample.rgb;
        return vec4(normalColor, 1.0);

    }
    if (diagnosticView == 3) {
        float shadowDepth = texture(shadowTexture, screenUv).r;
        float readableDepth = pow(clamp(shadowDepth, 0.0, 1.0), 4.0);
        return vec4(vec3(readableDepth), 1.0);

    }
    if (diagnosticView == 4) {
        float depth = texture(depthTexture, screenUv).r;
        float readableDepth = centerSample.a < 0.01
            ? 0.0
            : 1.0 - pow(clamp(depth, 0.0, 1.0), 32.0);
        return vec4(vec3(readableDepth), 1.0);

    }
    float edge = 0.0;
    if (centerSample.a >= 0.01 && outline.strength > 0.0) {
        vec3 centerNormal = normalize(centerSample.xyz * 2.0 - 1.0);
        float centerDepth = linearDepth(texture(depthTexture, screenUv).r);
    const ivec2 offsets[8] = ivec2[](
        ivec2(-1, 0),
        ivec2(1, 0),
        ivec2(0, -1),
        ivec2(0, 1),
        ivec2(-1, -1),
        ivec2(1, -1),
        ivec2(-1, 1),
        ivec2(1, 1));
        float maximumDepthDifference = 0.0;
        float maximumNormalDifference = 0.0;
        for (int index = 0; index < 8; ++index) {
            vec2 sampleUv = clamp(
                screenUv + vec2(offsets[index]) * texelSize,
                texelSize * 0.5,
                vec2(1.0) - texelSize * 0.5);
            vec4 neighbourSample = texture(normalTexture, sampleUv);
            if (neighbourSample.a < 0.01) {
                continue;
            }
            float participation = min(centerSample.a, neighbourSample.a);
            vec3 neighbourNormal =
                normalize(neighbourSample.xyz * 2.0 - 1.0);
            float neighbourDepth = linearDepth(texture(depthTexture, sampleUv).r);
            maximumDepthDifference = max(
                maximumDepthDifference,
                abs(centerDepth - neighbourDepth) / max(centerDepth,.001) * 100.0
                    * participation);
            maximumNormalDifference = max(
                maximumNormalDifference,
                (1.0 - max(dot(centerNormal, neighbourNormal), 0.0))
                    * participation);
        }
        float depthEdge = smoothstep(
            outline.depthThreshold,
            outline.depthThreshold + 0.28,
            maximumDepthDifference);
        float normalEdge = smoothstep(
            outline.normalThreshold,
            outline.normalThreshold + 0.22,
            maximumNormalDifference);
        edge = max(depthEdge, normalEdge) * outline.strength;
    }
    if (diagnosticView == 2) {
        vec3 background = vec3(0.018, 0.024, 0.040);
        vec3 edgeColor = vec3(0.35, 0.86, 1.0);
        return vec4(mix(background, edgeColor, edge), 1.0);
    } else {
        vec3 hdrColor = texture(sceneColorTexture, screenUv).rgb;
        vec3 bloom = vec3(0.0);
        if (outline.bloomStrength > 0.0 || outline.bloomIsolation > 0.5) {
            const vec2 bloomOffsets[8] = vec2[](
                vec2(-2.0, 0.0), vec2(2.0, 0.0),
                vec2(0.0, -2.0), vec2(0.0, 2.0),
                vec2(-1.4, -1.4), vec2(1.4, -1.4),
                vec2(-1.4, 1.4), vec2(1.4, 1.4));
            for (int index = 0; index < 8; ++index) {
                vec3 sampleColor = texture(
                    sceneColorTexture,
                    clamp(screenUv + bloomOffsets[index] * texelSize * 2.0,
                          texelSize * 0.5,
                          vec2(1.0) - texelSize * 0.5)).rgb;
                bloom += max(
                    sampleColor - vec3(outline.gradeParameters.z),
                    vec3(0.0));
            }
            bloom *= 0.125 * outline.bloomStrength;
        }
        if (outline.bloomIsolation > 0.5) {
            vec3 bloomDisplay = clamp(bloom * 4.0, vec3(0.0), vec3(1.0));
            return vec4(bloomDisplay, 1.0);

        }
        hdrColor += bloom;
        hdrColor = mix(hdrColor, outline.outlineColor.rgb, edge);
        hdrColor *= exp2(outline.exposureEv);
        vec3 displayLinear = outline.toneMappingEnabled > 0.5
            ? acesFitted(hdrColor)
            : clamp(hdrColor, 0.0, 1.0);
        displayLinear *= outline.gradeTint.rgb;
        float luminance = dot(
            displayLinear,
            vec3(0.2126, 0.7152, 0.0722));
        displayLinear = mix(
            vec3(luminance),
            displayLinear,
            outline.gradeParameters.x);
        displayLinear = (displayLinear - vec3(0.5))
            * outline.gradeParameters.y + vec3(0.5);
        displayLinear = clamp(displayLinear, vec3(0.0), vec3(1.0));
        return vec4(displayLinear, 1.0);
    }
}

float luma(vec3 rgb){return dot(rgb,vec3(.2126,.7152,.0722));}
void main(){
    vec2 texel=1.0/vec2(textureSize(sceneColorTexture,0));
    if(outline.sampling.x>1.5){
        vec4 sum=vec4(0);
        for(int y=0;y<2;++y)for(int x=0;x<2;++x)
            sum+=shadePixel(screenUv+(vec2(x,y)-vec2(.5))*texel);
        outputColor=sum*.25;
    }else if(outline.sampling.x>.5 && outline.diagnosticView<.5){
        vec4 center=shadePixel(screenUv);
        vec3 nw=shadePixel(screenUv+vec2(-1,-1)*texel).rgb;
        vec3 ne=shadePixel(screenUv+vec2(1,-1)*texel).rgb;
        vec3 sw=shadePixel(screenUv+vec2(-1,1)*texel).rgb;
        vec3 se=shadePixel(screenUv+vec2(1,1)*texel).rgb;
        float lc=luma(center.rgb),lnw=luma(nw),lne=luma(ne),lsw=luma(sw),lse=luma(se);
        float lo=min(lc,min(min(lnw,lne),min(lsw,lse))),hi=max(lc,max(max(lnw,lne),max(lsw,lse)));
        if(hi-lo<max(.025,hi*.125)){outputColor=center;return;}
        vec2 direction=vec2(-((lnw+lne)-(lsw+lse)),(lnw+lsw)-(lne+lse));
        float reduce=max((lnw+lne+lsw+lse)*(.25*.125),.0078125);
        direction=clamp(direction/(min(abs(direction.x),abs(direction.y))+reduce),vec2(-8),vec2(8))*texel;
        vec3 a=.5*(shadePixel(screenUv+direction*(1.0/3.0-.5)).rgb+shadePixel(screenUv+direction*(2.0/3.0-.5)).rgb);
        vec3 b=a*.5+.25*(shadePixel(screenUv-direction*.5).rgb+shadePixel(screenUv+direction*.5).rgb);
        float lb=luma(b);outputColor=vec4(lb<lo||lb>hi?a:b,1);
    }else outputColor=shadePixel(screenUv);
}
