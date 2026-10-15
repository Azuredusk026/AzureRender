// Isotropic GGX + energy-weighted Lambert. Directions point away from the surface.
vec3 fresnelSchlick(vec3 f0,float cosine){return f0+(1-f0)*pow(1-clamp(cosine,0,1),5);}
float ggxD(float nh,float rough){float a=rough*rough,a2=a*a,d=nh*nh*(a2-1)+1;return a2/(PI*d*d);}
float smithG1(float cosine,float rough){float a=rough*rough;return 2*cosine/max(cosine+sqrt(a*a+(1-a*a)*cosine*cosine),1e-8);}
float specularProbability(vec3 albedo,float metal){vec3 f0=mix(vec3(.04),albedo,metal);return clamp(max(max(f0.r,f0.g),f0.b),.1,.9);}
vec3 evaluateBsdf(vec3 n,vec3 v,vec3 l,vec3 albedo,float metal,float rough){
 float nv=dot(n,v),nl=dot(n,l);if(nv<=0||nl<=0)return vec3(0);
 vec3 h=normalize(v+l);float nh=max(dot(n,h),0);vec3 f=fresnelSchlick(mix(vec3(.04),albedo,metal),max(dot(v,h),0));
 return (1-f)*(1-metal)*albedo/PI+f*ggxD(nh,rough)*smithG1(nv,rough)*smithG1(nl,rough)/max(4*nv*nl,1e-8);
}
float bsdfPdf(vec3 n,vec3 v,vec3 l,vec3 albedo,float metal,float rough){
 if(dot(n,l)<=0||dot(n,v)<=0)return 0;
 vec3 h=normalize(v+l);float probability=specularProbability(albedo,metal);
 return (1-probability)*max(dot(n,l),0)/PI+probability*ggxD(max(dot(n,h),0),rough)*max(dot(n,h),0)/max(4*abs(dot(v,h)),1e-8);
}
mat3 surfaceBasis(vec3 n){vec3 t=normalize(cross(abs(n.y)<.99?vec3(0,1,0):vec3(1,0,0),n));return mat3(t,cross(n,t),n);}
vec3 sampleBsdf(vec3 n,vec3 v,vec3 albedo,float metal,float rough){
 mat3 basis=surfaceBasis(n);
 if(random()<specularProbability(albedo,metal)){
  float a=rough*rough,u=random(),phi=2*PI*random();float c=sqrt((1-u)/(1+(a*a-1)*u));
  vec3 h=basis*vec3(sqrt(max(0,1-c*c))*cos(phi),sqrt(max(0,1-c*c))*sin(phi),c);return reflect(-v,h);
 }
 float r=sqrt(random()),phi=2*PI*random();return basis*vec3(r*cos(phi),r*sin(phi),sqrt(max(0,1-r*r)));
}
