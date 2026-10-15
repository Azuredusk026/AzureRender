// Isotropic GGX + energy-weighted Lambert. Directions point away from the surface.
float materialTransmission=0,materialIor=1.5,materialEta=1.5;
float dielectricFresnel(float cosine,float eta){float c=abs(cosine),sinT2=(1-c*c)/(eta*eta);if(sinT2>=1)return 1;float ct=sqrt(max(0,1-sinT2));float rs=(c-eta*ct)/(c+eta*ct),rp=(eta*c-ct)/(eta*c+ct);return (rs*rs+rp*rp)*.5;}
vec3 fresnelSchlick(vec3 f0,float cosine){return f0+(1-f0)*pow(1-clamp(cosine,0,1),5);}
float ggxD(float nh,float rough){float a=rough*rough,a2=a*a,d=nh*nh*(a2-1)+1;return a2/(PI*d*d);}
float smithG1(float cosine,float rough){float a=rough*rough;return 2*cosine/max(cosine+sqrt(a*a+(1-a*a)*cosine*cosine),1e-8);}
float specularProbability(vec3 albedo,float metal){vec3 f0=mix(vec3(.04),albedo,metal);return clamp(max(max(f0.r,f0.g),f0.b),.1,.9);}
vec3 baseF0(vec3 albedo,float metal){float f=(materialIor-1)/(materialIor+1);return mix(vec3(f*f),albedo,metal);}
vec3 evaluateBsdf(vec3 n,vec3 v,vec3 l,vec3 albedo,float metal,float rough){
 float nv=dot(n,v),nl=dot(n,l);if(nv<=0||abs(nl)<1e-7)return vec3(0);
 if(nl<0){if(materialTransmission<=0||metal>=1||abs(materialEta-1)<1e-5)return vec3(0);
  vec3 h=normalize(v+materialEta*l);if(dot(h,n)<0)h=-h;float vh=dot(v,h),lh=dot(l,h);if(vh*lh>=0)return vec3(0);
  float denom=vh+materialEta*lh;float f=dielectricFresnel(vh,materialEta);
  return albedo*(1-metal)*materialTransmission*(1-f)*ggxD(max(dot(n,h),0),rough)*smithG1(nv,rough)*smithG1(abs(nl),rough)*abs(vh*lh)/max(nv*abs(nl)*denom*denom,1e-12);
 }
 vec3 h=normalize(v+l);float nh=max(dot(n,h),0),vh=max(dot(v,h),0);vec3 f=mix(vec3(dielectricFresnel(vh,materialEta)),fresnelSchlick(albedo,vh),metal);
 return (1-f)*(1-metal)*(1-materialTransmission)*albedo/PI+f*ggxD(nh,rough)*smithG1(nv,rough)*smithG1(nl,rough)/max(4*nv*nl,1e-8);
}
float bsdfPdf(vec3 n,vec3 v,vec3 l,vec3 albedo,float metal,float rough){
 float nl=dot(n,l);if(dot(n,v)<=0||abs(nl)<1e-7)return 0;
 float spec=specularProbability(albedo,metal),trans=(1-spec)*materialTransmission*(1-metal),diff=1-spec-trans;
 if(nl<0){if(trans<=0||abs(materialEta-1)<1e-5)return 0;vec3 h=normalize(v+materialEta*l);if(dot(h,n)<0)h=-h;
  float vh=dot(v,h),lh=dot(l,h),denom=vh+materialEta*lh;if(vh*lh>=0)return 0;
  return trans*ggxD(max(dot(n,h),0),rough)*max(dot(n,h),0)*abs(materialEta*materialEta*lh)/max(denom*denom,1e-12);
 }
 vec3 h=normalize(v+l);bool tir=length(refract(-v,h,1/materialEta))<.1;
 return diff*nl/PI+(spec+(tir?trans:0))*ggxD(max(dot(n,h),0),rough)*max(dot(n,h),0)/max(4*abs(dot(v,h)),1e-8);
}
mat3 surfaceBasis(vec3 n){vec3 t=normalize(cross(abs(n.y)<.99?vec3(0,1,0):vec3(1,0,0),n));return mat3(t,cross(n,t),n);}
vec3 sampleBsdf(vec3 n,vec3 v,vec3 albedo,float metal,float rough){
 mat3 basis=surfaceBasis(n);float spec=specularProbability(albedo,metal),trans=(1-spec)*materialTransmission*(1-metal),choice=random();
 if(choice<spec+trans){
  float a=rough*rough,u=random(),phi=2*PI*random();float c=sqrt((1-u)/(1+(a*a-1)*u));
  vec3 h=basis*vec3(sqrt(max(0,1-c*c))*cos(phi),sqrt(max(0,1-c*c))*sin(phi),c);if(dot(v,h)<=0)return -n;
  if(choice>=spec){vec3 l=refract(-v,h,1/materialEta);if(length(l)>.1)return l;}
  return reflect(-v,h);
 }
 float r=sqrt(random()),phi=2*PI*random();return basis*vec3(r*cos(phi),r*sin(phi),sqrt(max(0,1-r*r)));
}
