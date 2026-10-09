#include "PrimitiveGenerator.hpp"
#include "assets/GeneratorRegistry.hpp"
#include <array>
#include <cmath>
#include <cstring>
namespace azurerender {
namespace {
using Vec=std::array<float,3>;
constexpr float pi=3.14159265358979323846F;
struct Mesh {
    std::vector<float> vertices;
    std::vector<std::uint32_t> indices;
    Vec minimum{1e20F,1e20F,1e20F},maximum{-1e20F,-1e20F,-1e20F};
    std::uint32_t vertex(Vec position,Vec normal,float u,float v){
        const auto id=static_cast<std::uint32_t>(vertices.size()/8);float length=0;for(float x:normal)length+=x*x;length=std::sqrt(length);
        for(int axis=0;axis<3;++axis){minimum[axis]=std::min(minimum[axis],position[axis]);maximum[axis]=std::max(maximum[axis],position[axis]);vertices.push_back(position[axis]);}
        for(float x:normal)vertices.push_back(x/length);vertices.push_back(u);vertices.push_back(v);return id;
    }
    void triangle(std::uint32_t a,std::uint32_t b,std::uint32_t c){indices.insert(indices.end(),{a,b,c});}
};
void grid(Mesh& mesh,const Vec& size,const Vec& center,const Vec& tangent,const Vec& bitangent,const Vec& normal,unsigned segments){
    const auto base=static_cast<std::uint32_t>(mesh.vertices.size()/8);
    for(unsigned y=0;y<=segments;++y)for(unsigned x=0;x<=segments;++x){
        const float u=static_cast<float>(x)/segments,v=static_cast<float>(y)/segments;Vec position;
        for(int axis=0;axis<3;++axis)position[axis]=(center[axis]+(u-.5F)*tangent[axis]+(v-.5F)*bitangent[axis])*size[axis];
        mesh.vertex(position,normal,u,v);
    }
    for(unsigned y=0;y<segments;++y)for(unsigned x=0;x<segments;++x){const auto a=base+y*(segments+1)+x,b=a+segments+1;mesh.triangle(a,a+1,b);mesh.triangle(a+1,b+1,b);}
}
void word(std::string& bytes,std::uint32_t value){for(unsigned shift=0;shift<32;shift+=8)bytes.push_back(static_cast<char>(value>>shift));}
std::string encode(const Mesh& mesh){
    const auto vertexBytes=mesh.vertices.size()*sizeof(float),indexBytes=mesh.indices.size()*sizeof(std::uint32_t);
    std::string binary(vertexBytes+indexBytes,'\0');std::memcpy(binary.data(),mesh.vertices.data(),vertexBytes);std::memcpy(binary.data()+vertexBytes,mesh.indices.data(),indexBytes);
    const auto count=mesh.vertices.size()/8;
    nlohmann::json document={
        {"asset",{{"version","2.0"},{"generator","Azure primitive generator v1"}}},
        {"buffers",nlohmann::json::array({{{"byteLength",binary.size()}}})},
        {"bufferViews",nlohmann::json::array({{{"buffer",0},{"byteOffset",0},{"byteLength",vertexBytes},{"byteStride",32},{"target",34962}},{{"buffer",0},{"byteOffset",vertexBytes},{"byteLength",indexBytes},{"target",34963}}})},
        {"accessors",nlohmann::json::array({{{"bufferView",0},{"byteOffset",0},{"componentType",5126},{"count",count},{"type","VEC3"},{"min",mesh.minimum},{"max",mesh.maximum}},{{"bufferView",0},{"byteOffset",12},{"componentType",5126},{"count",count},{"type","VEC3"}},{{"bufferView",0},{"byteOffset",24},{"componentType",5126},{"count",count},{"type","VEC2"}},{{"bufferView",1},{"componentType",5125},{"count",mesh.indices.size()},{"type","SCALAR"}}})},
        {"materials",nlohmann::json::array({{{"name","Default PBR"},{"pbrMetallicRoughness",{{"baseColorFactor",{.65,.68,.72,1.}},{"metallicFactor",0},{"roughnessFactor",.7}}}}})},
        {"meshes",nlohmann::json::array({{{"primitives",nlohmann::json::array({{{"attributes",{{"POSITION",0},{"NORMAL",1},{"TEXCOORD_0",2}}},{"indices",3},{"material",0},{"mode",4}}})}}})},
        {"nodes",nlohmann::json::array({{{"name","Primitive"},{"mesh",0}}})},
        {"scenes",nlohmann::json::array({{{"nodes",{0}}}})},{"scene",0}};
    auto json=document.dump();while(json.size()%4)json+=' ';while(binary.size()%4)binary+='\0';
    std::string result;word(result,0x46546c67);word(result,2);word(result,static_cast<std::uint32_t>(28+json.size()+binary.size()));
    word(result,static_cast<std::uint32_t>(json.size()));word(result,0x4e4f534a);result+=json;
    word(result,static_cast<std::uint32_t>(binary.size()));word(result,0x004e4942);result+=binary;return result;
}
std::string generate(const GenerationRequest& request){
    const auto& args=request.parameters;const auto kind=args.value("kind",std::string("cube"));
    const Vec size=args.value("size",Vec{1,1,1});
    for(float value:size)if(!std::isfinite(value)||value<=0||value>10000)throw std::invalid_argument("Primitive dimensions must be positive and at most 10000");
    if(args.contains("segments")&&!args.at("segments").is_number_integer())throw std::invalid_argument("Primitive segments require an integer");
    const auto inputSegments=args.value("segments",12);if(inputSegments<1||inputSegments>128)throw std::invalid_argument("Primitive segments require 1..128");
    const auto segments=static_cast<unsigned>(inputSegments);Mesh mesh;
    if(kind=="plane")grid(mesh,size,{0,0,0},{1,0,0},{0,0,-1},{0,1,0},segments);
    else if(kind=="cube"){
        grid(mesh,size,{.5F,0,0},{0,0,-1},{0,1,0},{1,0,0},segments);
        grid(mesh,size,{-.5F,0,0},{0,0,1},{0,1,0},{-1,0,0},segments);
        grid(mesh,size,{0,.5F,0},{1,0,0},{0,0,-1},{0,1,0},segments);
        grid(mesh,size,{0,-.5F,0},{1,0,0},{0,0,1},{0,-1,0},segments);
        grid(mesh,size,{0,0,.5F},{1,0,0},{0,1,0},{0,0,1},segments);
        grid(mesh,size,{0,0,-.5F},{-1,0,0},{0,1,0},{0,0,-1},segments);
    }else if(kind=="sphere"){
        if(segments<4||segments%4)throw std::invalid_argument("Sphere segments require a multiple of four from 4..128");
        for(unsigned y=0;y<=segments;++y)for(unsigned x=0;x<=segments;++x){
            if(request.check)request.check();const float u=static_cast<float>(x)/segments,v=static_cast<float>(y)/segments;
            const Vec unit{std::sin(v*pi)*std::cos(u*2*pi),std::cos(v*pi),std::sin(v*pi)*std::sin(u*2*pi)};Vec position,normal;
            for(int axis=0;axis<3;++axis){position[axis]=unit[axis]*size[axis]*.5F;normal[axis]=unit[axis]/size[axis];}mesh.vertex(position,normal,u,v);
        }
        for(unsigned y=0;y<segments;++y)for(unsigned x=0;x<segments;++x){const auto a=y*(segments+1)+x,b=a+segments+1;if(y)mesh.triangle(a,a+1,b);if(y+1<segments)mesh.triangle(a+1,b+1,b);}
    }else if(kind=="cylinder"){
        if(segments<4||segments%4)throw std::invalid_argument("Cylinder segments require a multiple of four from 4..128");
        for(unsigned y=0;y<=segments;++y)for(unsigned x=0;x<=segments;++x){const float u=static_cast<float>(x)/segments,v=static_cast<float>(y)/segments,c=std::cos(u*2*pi),s=std::sin(u*2*pi);mesh.vertex({c*size[0]*.5F,(v-.5F)*size[1],s*size[2]*.5F},{c/size[0],0,s/size[2]},u,v);}
        for(unsigned y=0;y<segments;++y)for(unsigned x=0;x<segments;++x){const auto a=y*(segments+1)+x,b=a+segments+1;mesh.triangle(a,b,a+1);mesh.triangle(a+1,b,b+1);}
        for(int sign:{-1,1}){const auto center=mesh.vertex({0,sign*size[1]*.5F,0},{0,static_cast<float>(sign),0},.5F,.5F),first=center+1;
            for(unsigned x=0;x<=segments;++x){const float angle=static_cast<float>(x)/segments*2*pi,c=std::cos(angle),s=std::sin(angle);mesh.vertex({c*size[0]*.5F,sign*size[1]*.5F,s*size[2]*.5F},{0,static_cast<float>(sign),0},c*.5F+.5F,s*.5F+.5F);}
            for(unsigned x=0;x<segments;++x)if(sign<0)mesh.triangle(center,first+x,first+x+1);else mesh.triangle(center,first+x+1,first+x);
        }
    }else throw std::invalid_argument("Unknown primitive kind");
    return encode(mesh);
}
}
void registerPrimitiveGenerator(GeneratorRegistry& registry){registry.add("azure.primitive",1,generate);}
}
