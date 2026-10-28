#ifndef AZURE_FOG_ANALYTIC_GLSL
#define AZURE_FOG_ANALYTIC_GLSL
#include "fog_transport.glsl"
struct AzureAerialFog {vec4 rows[6];};
AzureFogTransport azureIdentityFog(){return AzureFogTransport(vec3(0),vec3(1));}
AzureFogTransport azureComposeFogTransport(AzureFogTransport front,AzureFogTransport back){
    return AzureFogTransport(front.scattering+front.transmittance*back.scattering,front.transmittance*back.transmittance);
}
float azureFogLogExprel(float x){
    if(abs(x)<.02){float x2=x*x;return log(1+x*.5+x2/6+x2*x/24+x2*x2/120);}
    if(x>50)return x-log(x)+log(1-exp(-x));
    if(x< -50)return log(1-exp(x))-log(-x);
    return log((exp(x)-1)/x);
}
float azureFogTransmission(float logarithm){
    if(logarithm>6)return 0;
    if(logarithm< -20)return 1;
    return exp(-exp(logarithm));
}
bool azureFogRay(vec3 start,vec3 end,out vec3 direction,out float logLength){
    vec3 halfDelta=end*.5-start*.5;
    float scale=max(max(abs(halfDelta.x),abs(halfDelta.y)),abs(halfDelta.z));
    direction=vec3(0);logLength=0;if(scale==0)return false;
    vec3 scaled=halfDelta/scale;float n=length(scaled);direction=scaled/n;logLength=log(scale)+log(n)+.6931471805599453;return true;
}
float azureFogHeightAverage(float falloff,float reference,float start,float end){
    if(falloff==0)return 0;
    float height=-(falloff*(min(start,end)*.5-reference*.5))*2;
    float dy=abs(end*.5-start*.5);if(dy==0)return height;
    float logQ=log(falloff)+log(dy)+.6931471805599453;
    return height+(logQ>3.912023005428146?-logQ:azureFogLogExprel(-exp(logQ)));
}
AzureFogTransport azureEvaluateHeightFog(vec3 parameters,vec3 color,vec3 start,vec3 end){
    vec3 direction;float logLength;
    if(parameters.x==0||!azureFogRay(start,end,direction,logLength))return azureIdentityFog();
    float optical=log(parameters.x)+logLength+azureFogHeightAverage(parameters.y,parameters.z,start.y,end.y);
    float t=azureFogTransmission(optical);return AzureFogTransport(color*(1-t),vec3(t));
}
AzureFogTransport azureEvaluateAerialFog(AzureAerialFog p,vec3 start,vec3 end){
    vec3 direction;float logLength;
    if(p.rows[0].w==0||!azureFogRay(start,end,direction,logLength))return azureIdentityFog();
    if(p.rows[3].w>0){float difference=log(p.rows[3].w)-logLength;if(difference>=0)return azureIdentityFog();logLength+=log(1-exp(difference));}
    float average=azureFogHeightAverage(p.rows[1].w,p.rows[2].w,start.y,end.y);
    if(p.rows[5].w>.5){
        float signedQ=-(p.rows[1].w*(end.y*.5-start.y*.5))*2;
        float q=max(signedQ,p.rows[5].z);float rel;
        if(q>50)rel=signedQ>=p.rows[5].z?-(log(p.rows[1].w)+log(abs(end.y*.5-start.y*.5))+.6931471805599453):-log(q);
        else rel=azureFogLogExprel(-q);
        average=-(p.rows[1].w*(end.y*.5-p.rows[2].w*.5))*2+rel;
    }
    float column=log(p.rows[0].w)+logLength+average;vec3 t;
    for(int i=0;i<3;++i)t[i]=p.rows[0][i]==0?1:azureFogTransmission(column+log(p.rows[0][i]));
    float cosine=clamp(dot(direction,p.rows[4].xyz),-1,1),g=p.rows[4].w;
    float rayleigh=.05968310365946075*(1+cosine*cosine);
    float denominator=12.566370614359172*pow(max(1+g*g-2*g*cosine,0),1.5);
    float mie=(1-g*g)/max(denominator,p.rows[5].y);
    vec3 color=clamp(p.rows[1].xyz*rayleigh+p.rows[2].xyz*mie+p.rows[3].xyz,0,1)*p.rows[5].x;
    return AzureFogTransport(color*(1-t),t);
}
#endif
