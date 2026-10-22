// Linear F0, perceptual roughness, and a clamped view cosine.
// The response weights prefiltered radiance; texture LOD remains caller-owned.
vec3 ggxMultiscatterEnvironment(vec3 f0,float roughness,float ndotv) {
    float r=clamp(roughness,0.0,1.0),v=clamp(ndotv,0.0,1.0);
    float r2=r*r,r6=r2*r2*r2,v2=v*v,v3=v2*v;
    float a=(3.327070*v+.036546+(-9.047560*v+9.063200)*r2)
        /(3.596850*v2-1.367720*v3+1.0
          +(-16.317400*v2+9.044010+9.229490*v3)*r2
          +(5.565890+19.788601*v2-20.212299*v3)*r6);
    float b=(-1.285140*v+.990440+(1.296780-.755907*v)*r2)
        /(2.923380*v+59.418800*v3+1.0
          +(20.322500-27.030199*v+222.591995*v3)*r2
          +(626.130005*v+316.627014*v3+121.563004)*r6);
    return (f0*a+b)*(vec3(1)+(1.0-a-b)/(a+b)*f0);
}
