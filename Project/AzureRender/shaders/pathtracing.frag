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
  for(int y=-2;y<=2;++y)for(int x=-2;x<=2;++x){ivec2 q=clamp(ivec2(pixel)+ivec2(x,y),ivec2(0),ivec2(p.image.xy)-1);uint neighbor=uint(q.y)*p.image.x+uint(q.x);vec4 g=guides[neighbor];
   float w=exp(-float(x*x+y*y)/4.0);if(guide.w<0||g.w<0)w*=float(guide.w==g.w);else w*=pow(max(dot(guide.xyz,g.xyz),0),32)*exp(-abs(guide.w-g.w)/max(.01,guide.w*.01));
   sum+=history[neighbor].rgb*w;weightSum+=w;
  }color.rgb=sum/max(weightSum,1e-8);
 }
}
