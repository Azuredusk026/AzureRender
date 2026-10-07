#include "ProceduralInternal.hpp"
#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>
namespace azurerender::procedural {
namespace {
std::string base64(const std::vector<unsigned char>& bytes){
    constexpr char alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;out.reserve((bytes.size()+2)/3*4);
    for(std::size_t i=0;i<bytes.size();i+=3){
        const unsigned n=(static_cast<unsigned>(bytes[i])<<16)|(i+1<bytes.size()?static_cast<unsigned>(bytes[i+1])<<8:0)|(i+2<bytes.size()?bytes[i+2]:0);
        out+=alphabet[(n>>18)&63];out+=alphabet[(n>>12)&63];out+=i+1<bytes.size()?alphabet[(n>>6)&63]:'=';out+=i+2<bytes.size()?alphabet[n&63]:'=';
    }
    return out;
}
template<class T>std::size_t buffer(GltfWriter& writer,const std::vector<T>& input,std::size_t stride,const std::string& type,unsigned componentType){
    if(input.empty()||input.size()%stride)throw std::invalid_argument("Generated accessor requires complete elements");
    while(writer.bytes.size()%4)writer.bytes.push_back(0);
    const auto offset=writer.bytes.size(),size=input.size()*sizeof(T);
    if(offset+size>96*1024*1024)throw std::invalid_argument("Generated embedded buffer budget exceeded");
    const auto* bytes=reinterpret_cast<const unsigned char*>(input.data());writer.bytes.insert(writer.bytes.end(),bytes,bytes+size);
    writer.data["bufferViews"].push_back({{"buffer",0},{"byteOffset",offset},{"byteLength",size}});
    const auto index=writer.data["accessors"].size();
    writer.data["accessors"].push_back({{"bufferView",writer.data["bufferViews"].size()-1},{"componentType",componentType},{"count",input.size()/stride},{"type",type}});
    return index;
}
}
std::size_t GltfWriter::accessor(const std::vector<float>& input,std::size_t stride,const std::string& type,bool bounds){
    for(const auto n:input)if(!std::isfinite(n))throw std::invalid_argument("Generated accessor contains non-finite values");
    const auto index=buffer(*this,input,stride,type,5126);
    if(bounds){
        std::vector<float> lo(stride,std::numeric_limits<float>::max()),hi(stride,std::numeric_limits<float>::lowest());
        for(std::size_t i=0;i<input.size();++i){lo[i%stride]=std::min(lo[i%stride],input[i]);hi[i%stride]=std::max(hi[i%stride],input[i]);}
        data["accessors"][index]["min"]=lo;data["accessors"][index]["max"]=hi;
    }
    return index;
}
std::size_t GltfWriter::indices(const std::vector<std::uint32_t>& input){return buffer(*this,input,1,"SCALAR",5125);}
std::size_t GltfWriter::joints(const std::vector<std::uint16_t>& input){return buffer(*this,input,4,"VEC4",5123);}
Json GltfWriter::primitive(const std::vector<vec3>& points,const std::vector<std::uint32_t>& triangles,std::size_t materialIndex,const std::vector<std::uint16_t>& owners){
    if(triangles.empty()||triangles.size()%3||(!owners.empty()&&owners.size()!=points.size()))throw std::invalid_argument("Generated mesh has incomplete ownership or triangles");
    std::vector<float> p,n,weights;std::vector<std::uint16_t> joints;std::vector<std::uint32_t> indices;
    for(std::size_t i=0;i<triangles.size();i+=3){
        const auto a=triangles[i],b=triangles[i+1],c=triangles[i+2];
        if(a>=points.size()||b>=points.size()||c>=points.size())throw std::invalid_argument("Generated mesh index outside vertices");
        const auto face=la::cross(points[b]-points[a],points[c]-points[a]);
        if(la::length(face)<=1e-12)throw std::invalid_argument("Generated triangle is degenerate");
        const auto normal=la::normalize(face);
        for(std::size_t corner=0;corner<3;++corner){
            const auto index=triangles[i+corner];
            for(int axis=0;axis<3;++axis){p.push_back(static_cast<float>(points[index][axis]));n.push_back(static_cast<float>(normal[axis]));}
            indices.push_back(static_cast<std::uint32_t>(indices.size()));
            if(!owners.empty()){joints.insert(joints.end(),{owners[index],0,0,0});weights.insert(weights.end(),{1,0,0,0});}
        }
    }
    Json attributes={{"POSITION",accessor(p,3,"VEC3",true)},{"NORMAL",accessor(n,3,"VEC3")}};
    if(!owners.empty()){attributes["JOINTS_0"]=this->joints(joints);attributes["WEIGHTS_0"]=accessor(weights,4,"VEC4");}
    return {{"attributes",attributes},{"indices",this->indices(indices)},{"material",materialIndex},{"mode",4}};
}
std::size_t GltfWriter::material(const Json& description){
    fields(description,{"color","metallic","roughness"});const auto color=description.value("color",Json::array({.5,.7,.8,1}));
    if(!color.is_array()||color.size()!=4)throw std::invalid_argument("Material color requires four components");
    for(const auto& component:color){const auto n=finiteNumber(component);if(n<0||n>1)throw std::invalid_argument("Material color outside 0..1");}
    const auto metallic=finiteNumber(description.value("metallic",Json(0))),roughness=finiteNumber(description.value("roughness",Json(.6)));
    if(metallic<0||metallic>1||roughness<0||roughness>1)throw std::invalid_argument("Material factors outside 0..1");
    const auto index=data["materials"].size();data["materials"].push_back({{"name","procedural_material_"+std::to_string(index)},
        {"pbrMetallicRoughness",{{"baseColorFactor",color},{"metallicFactor",metallic},{"roughnessFactor",roughness}}}});return index;
}
std::string GltfWriter::finish(){
    data["buffers"]=Json::array({{{"byteLength",bytes.size()},{"uri","data:application/octet-stream;base64,"+base64(bytes)}}});
    auto result=data.dump();if(result.size()>128*1024*1024)throw std::invalid_argument("Generated glTF exceeds output byte budget");return result;
}
}
