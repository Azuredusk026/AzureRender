#include "PrimitiveMesh.hpp"
#include <stdexcept>
namespace azurerender::primitives {
bool roundPrimitive(Mesh& mesh,const std::string& kind,const Vec& size,unsigned segments,const std::function<void()>& check){
    if(kind!="capsule"&&kind!="torus")return false;
    if(segments<4||segments%4)throw std::invalid_argument("Round primitive segments require a multiple of four from 4..128");
    constexpr float pi=3.14159265358979323846F;
    const unsigned rows=kind=="capsule"?segments+1:segments;
    for(unsigned y=0;y<=rows;++y)for(unsigned x=0;x<=segments;++x){
        if(check)check();const float u=static_cast<float>(x)/segments,angle=u*2*pi;
        const float c=std::cos(angle),s=std::sin(angle);
        if(kind=="capsule"){
            const auto half=segments/2;const bool top=y<=half;
            const float phi=top?static_cast<float>(y)/half*pi*.5F:pi*.5F+static_cast<float>(y-half-1)/half*pi*.5F;
            const float radius=std::sin(phi),height=std::cos(phi);
            mesh.vertex({radius*c*.5F*size[0],(height*.25F+(top?.25F:-.25F))*size[1],radius*s*.5F*size[2]},
                {radius*c/(size[0]*.5F),height/(size[1]*.25F),radius*s/(size[2]*.5F)},u,static_cast<float>(y)/rows);
        }else{
            const float v=static_cast<float>(y)/segments,phi=v*2*pi,cv=std::cos(phi),sv=std::sin(phi),radius=.375F+.125F*cv;
            mesh.vertex({radius*c*size[0],sv*.5F*size[1],radius*s*size[2]},
                {cv*c/(size[0]*.125F),sv/(size[1]*.5F),cv*s/(size[2]*.125F)},u,v);
        }
    }
    for(unsigned y=0;y<rows;++y)for(unsigned x=0;x<segments;++x){
        const auto a=y*(segments+1)+x,b=a+segments+1;
        if(kind=="torus"){mesh.triangle(a,b,a+1);mesh.triangle(a+1,b,b+1);}
        else{if(y)mesh.triangle(a,a+1,b);if(y+1<rows)mesh.triangle(a+1,b+1,b);}
    }
    return true;
}
}
