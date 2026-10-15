#version 450
layout(std140,binding=0) uniform Params {vec4 origin,right,up,forward,skyColor,sky;uvec4 counts,image;} p;
layout(std430,binding=5) readonly buffer History{vec4 history[];};
layout(std430,binding=8) readonly buffer Guides{vec4 guides[];};
layout(location=0) out vec4 color;
layout(location=1) out vec4 normal;
void main(){
 uvec2 pixel=min(uvec2(gl_FragCoord.xy),p.image.xy-1u);uint index=pixel.y*p.image.x+pixel.x;
 color=history[index];normal=vec4(.5,.5,1,0);
 if(p.sky.w>.5){vec4 guide=guides[index];vec3 sum=vec3(0);float weightSum=0;
  // Robust display-only luminance limit prevents isolated samples spreading
  // across the filter footprint. The radiance history remains untouched.
  float luminances[25];int count=0;
  for(int y=-2;y<=2;++y)for(int x=-2;x<=2;++x){ivec2 q=clamp(ivec2(pixel)+ivec2(x,y),ivec2(0),ivec2(p.image.xy)-1);uint neighbor=uint(q.y)*p.image.x+uint(q.x);vec4 g=guides[neighbor];
   bool compatible=guide.w<0||g.w<0?guide.w==g.w:dot(guide.xyz,g.xyz)>.8&&abs(guide.w-g.w)<max(.02,guide.w*.02);
   if(compatible){float value=dot(history[neighbor].rgb,vec3(.2126,.7152,.0722));int j=count;while(j>0&&luminances[j-1]>value){luminances[j]=luminances[j-1];--j;}luminances[j]=value;++count;}
  }
  float limit=max(.01,4.0*luminances[max(count/2,0)]);
  for(int y=-2;y<=2;++y)for(int x=-2;x<=2;++x){ivec2 q=clamp(ivec2(pixel)+ivec2(x,y),ivec2(0),ivec2(p.image.xy)-1);uint neighbor=uint(q.y)*p.image.x+uint(q.x);vec4 g=guides[neighbor];
   float w=exp(-float(x*x+y*y)/4.0);if(guide.w<0||g.w<0)w*=float(guide.w==g.w);else w*=pow(max(dot(guide.xyz,g.xyz),0),32)*exp(-abs(guide.w-g.w)/max(.01,guide.w*.01));
   vec3 radiance=history[neighbor].rgb;float luminance=dot(radiance,vec3(.2126,.7152,.0722));radiance*=min(1.0,limit/max(luminance,1e-8));
   sum+=radiance*w;weightSum+=w;
  }color.rgb=sum/max(weightSum,1e-8);
 }
}
