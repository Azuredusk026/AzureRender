#ifndef AZURE_IRRADIANCE_CLIPMAP_GLSL
#define AZURE_IRRADIANCE_CLIPMAP_GLSL
struct IrradianceClipmapLayerSample {vec3 amplitude;vec4 red;vec4 green;vec4 blue;};
struct IrradianceClipmapResult {mat3x4 coefficients;mat3 alpha;};
IrradianceClipmapLayerSample sampleIrradianceClipmapLayer(sampler3D amplitude,sampler3D directional,
    vec3 dimensions,vec3 position,float spacing){
    IrradianceClipmapLayerSample result;
    vec3 uv=fract((position/spacing+.5)/dimensions);
    result.amplitude=textureLod(amplitude,uv,0).rgb;
    uv.y=clamp(uv.y,.5/dimensions.y,1.0-.5/dimensions.y)/3.0;
    result.red=textureLod(directional,uv,0);
    result.green=textureLod(directional,uv+vec3(0,1.0/3.0,0),0);
    result.blue=textureLod(directional,uv+vec3(0,2.0/3.0,0),0);
    return result;
}
IrradianceClipmapResult evaluateIrradianceClipmap(sampler3D textures[6],vec4 parameters[13],vec3 world){
    vec3 position=vec3(dot(parameters[1],vec4(world,1)),dot(parameters[2],vec4(world,1)),dot(parameters[3],vec4(world,1)));
    vec3 fades;
    for(int i=0;i<3;++i){vec3 distance=abs(position-parameters[7+2*i].xyz);vec3 fade=parameters[8+2*i].xyz;
        fades[i]=max(clamp((max(distance.x,distance.z)-fade.x)/fade.z,0,1),clamp((distance.y-fade.y)/fade.z,0,1));}
    IrradianceClipmapResult result;result.alpha=mat3(0);
    mat3x4 coefficients=mat3x4(parameters[4],parameters[5],parameters[6]);
    if(parameters[0].w>.5&&fades.z<1){
        vec3 weights=vec3(1-fades.x,fades.x*(1-fades.y),fades.y*(1-fades.z));
        IrradianceClipmapLayerSample samples[3];
        for(int i=0;i<3;++i){samples[i].amplitude=vec3(0);samples[i].red=samples[i].green=samples[i].blue=vec4(0);}
        // Constant descriptor indices avoid sampled-image dynamic indexing.
        if(weights.x>0)samples[0]=sampleIrradianceClipmapLayer(textures[0],textures[1],parameters[0].xyz,position,parameters[7].w);
        if(weights.y>0)samples[1]=sampleIrradianceClipmapLayer(textures[2],textures[3],parameters[0].xyz,position,parameters[9].w);
        if(weights.z>0)samples[2]=sampleIrradianceClipmapLayer(textures[4],textures[5],parameters[0].xyz,position,parameters[11].w);
        float alpha=fades.z;
        for(int i=0;i<3;++i){alpha+=weights[i]*samples[i].red.a;
            result.alpha[i]=vec3(samples[i].red.a,samples[i].green.a,samples[i].blue.a);}
        float remapped=clamp(2*alpha-1,0,1),u=remapped-fades.z,v=.5*(remapped+fades.z);
        for(int c=0;c<3;++c){vec4 color=vec4(0);
            for(int i=0;i<3;++i){vec4 direction=c==0?samples[i].red:(c==1?samples[i].green:samples[i].blue);
                color+=weights[i]*samples[i].amplitude[c]*vec4(4*direction.xyz-2,1);}
            vec4 defaults=parameters[4+c];color+=v*defaults;
            color.y+=.5*u*defaults.w;color.w+=.375*u*defaults.y;coefficients[c]=color;
        }
    }
    for(int c=0;c<3;++c)result.coefficients[c]=vec4(
        parameters[1].xyz*coefficients[c].x+parameters[2].xyz*coefficients[c].y+parameters[3].xyz*coefficients[c].z,
        coefficients[c].w);
    return result;
}
#endif
