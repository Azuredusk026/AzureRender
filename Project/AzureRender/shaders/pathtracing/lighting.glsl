float powerWeight(float a,float b){return a*a/max(a*a+b*b,1e-20);}
vec3 rotateEnvironment(vec3 d,float a){d.xz=mat2(cos(a),sin(a),-sin(a),cos(a))*d.xz;return d;}
float environmentPdf(vec3 direction){
 vec3 d=rotateEnvironment(direction,p.sky.z);uint x=min(63u,uint((atan(d.z,d.x)/(2*PI)+.5)*64));uint y=min(31u,uint(acos(clamp(d.y,-1,1))/PI*32));
 float area=2*PI/64*(cos(PI*float(y)/32)-cos(PI*float(y+1)/32));return uintBitsToFloat(pixels[2048u+y*64u+x])/area;
}
vec3 sampleEnvironment(){
 float u=random();uint lo=0,hi=2047;while(lo<hi){uint mid=(lo+hi)/2;if(uintBitsToFloat(pixels[mid])<u)lo=mid+1;else hi=mid;}
 uint x=lo%64u,y=lo/64u;float phi=(float(x)+random())/64*2*PI-PI;
 float c=mix(cos(PI*float(y)/32),cos(PI*float(y+1)/32),random());
 return rotateEnvironment(vec3(sqrt(max(0,1-c*c))*cos(phi),c,sqrt(max(0,1-c*c))*sin(phi)),-p.sky.z);
}
float areaPdf(uint index,vec3 previous,vec3 position,vec3 direction){
 if(p.counts.z==0u)return 0;
 Triangle t=triangles[index];float area=length(cross(t.e1.xyz,t.e2.xyz))*.5;
 float cosine=abs(dot(normalize(cross(t.e1.xyz,t.e2.xyz)),-direction));
 return dot(position-previous,position-previous)/max(area*cosine*float(p.counts.z),1e-12);
}
bool visible(vec3 position,vec3 ng,vec3 direction,float distance){uint id;vec2 uv,bary;return !intersect(position+ng*.001,direction,distance,id,uv,bary);}
vec3 directLighting(vec3 position,vec3 ng,vec3 n,vec3 v,vec3 albedo,float metal,float rough){
 vec3 result=vec3(0);
 if(p.counts.z>0u){uint chosen=min(p.counts.z-1u,uint(random()*float(p.counts.z)));Light light=lights[chosen];vec3 target=light.position.xyz;vec3 radiance=light.radiance.rgb;float pdf=1/float(p.counts.z);bool area=light.position.w<.5;
  if(area){uint index=uint(light.position.x);Triangle t=triangles[index];float u=sqrt(random()),w=random();vec2 b=vec2(u*(1-w),u*w);target=t.p0.xyz+t.e1.xyz*b.x+t.e2.xyz*b.y;Material m=materials[uint(t.uv2material.z)];vec2 uv=t.uv01.xy*(1-b.x-b.y)+t.uv01.zw*b.x+t.uv2material.xy*b.y;radiance=texel(m.emission,uv).rgb*m.properties.x;}
  vec3 delta=target-position;float distance=length(delta);vec3 l=delta/max(distance,1e-8);float cosine=max(dot(n,l),0);
  if(!area&&light.coneType.y>.5){float cone=clamp((dot(-l,light.directionOuter.xyz)-light.directionOuter.w)/max(light.coneType.x-light.directionOuter.w,1e-6),0,1);if(light.coneType.x==light.directionOuter.w)cone=step(light.directionOuter.w,dot(-l,light.directionOuter.xyz));radiance*=cone*cone;}
  if(cosine>0&&dot(ng,l)>0&&visible(position,ng,l,distance-.002)){
   if(area)pdf=areaPdf(uint(light.position.x),position,target,l);
   else radiance*=light.coneType.z>.5?localLightDistanceWeight(distance*distance,light.radiance.w,light.coneType.z,light.coneType.w):1.0/max(distance*distance,.01);
   float weight=area?powerWeight(pdf,bsdfPdf(n,v,l,albedo,metal,rough)):1;
   result+=evaluateBsdf(n,v,l,albedo,metal,rough)*radiance*cosine*weight/max(pdf,1e-12);
  }
 }
 vec3 l=sampleEnvironment();float cosine=max(dot(n,l),0),pdf=environmentPdf(l);
 if(cosine>0&&dot(ng,l)>0&&visible(position,ng,l,1e30))result+=evaluateBsdf(n,v,l,albedo,metal,rough)*sky(l)*cosine*powerWeight(pdf,bsdfPdf(n,v,l,albedo,metal,rough))/max(pdf,1e-12);
 return result;
}
