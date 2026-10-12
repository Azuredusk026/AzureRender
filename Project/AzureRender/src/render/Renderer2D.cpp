#include "render/Renderer2D.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace azurerender {
void Renderer2D::quad(std::array<std::array<float,2>,4> points,std::array<float,4> color,Rml::TextureHandle texture,int layer){
    if(pending_.size()>=10000)throw std::invalid_argument("2D primitive budget exceeded");
    for(const auto& point:points)for(float value:point)if(!std::isfinite(value))throw std::invalid_argument("Non-finite 2D coordinate");
    for(float value:color)if(!std::isfinite(value)||value<0||value>1)throw std::invalid_argument("2D color outside [0,1]");
    Primitive primitive{};primitive.texture=texture;primitive.layer=layer;
    const std::array<std::array<float,2>,4> uv={{{0,0},{1,0},{1,1},{0,1}}};
    for(std::size_t i=0;i<4;++i){auto& vertex=primitive.vertices[i];vertex.position={points[i][0],points[i][1]};vertex.tex_coord={uv[i][0],uv[i][1]};vertex.colour={static_cast<Rml::byte>(std::lround(color[0]*color[3]*255)),static_cast<Rml::byte>(std::lround(color[1]*color[3]*255)),static_cast<Rml::byte>(std::lround(color[2]*color[3]*255)),static_cast<Rml::byte>(std::lround(color[3]*255))};}
    pending_.push_back(primitive);
}
void Renderer2D::sprite(Rect2D rect,std::array<float,4> color,Rml::TextureHandle texture,int layer){
    if(!std::isfinite(rect.width)||!std::isfinite(rect.height)||rect.width<=0||rect.height<=0)throw std::invalid_argument("Invalid 2D sprite size");
    quad({{{rect.x,rect.y},{rect.x+rect.width,rect.y},{rect.x+rect.width,rect.y+rect.height},{rect.x,rect.y+rect.height}}},color,texture,layer);
}
void Renderer2D::line(std::array<float,2> from,std::array<float,2> to,float width,std::array<float,4> color,int layer){
    const float dx=to[0]-from[0],dy=to[1]-from[1],length=std::hypot(dx,dy);
    if(!std::isfinite(width)||width<=0||!std::isfinite(length)||length<=0)throw std::invalid_argument("Invalid 2D line");
    const float x=-dy/length*width*.5F,y=dx/length*width*.5F;
    quad({{{from[0]+x,from[1]+y},{to[0]+x,to[1]+y},{to[0]-x,to[1]-y},{from[0]-x,from[1]-y}}},color,0,layer);
}
Renderer2DStatistics Renderer2D::flush(){
    auto primitives=std::move(pending_);pending_.clear();std::stable_sort(primitives.begin(),primitives.end(),[](const auto& a,const auto& b){return a.layer<b.layer;});
    Renderer2DStatistics statistics;statistics.primitives=primitives.size();
    for(std::size_t begin=0;begin<primitives.size();){
        std::size_t end=begin+1;while(end<primitives.size()&&primitives[end].texture==primitives[begin].texture)++end;
        std::vector<Rml::Vertex> vertices;std::vector<int> indices;vertices.reserve((end-begin)*4);indices.reserve((end-begin)*6);
        for(std::size_t i=begin;i<end;++i){const auto base=static_cast<int>(vertices.size());vertices.insert(vertices.end(),primitives[i].vertices.begin(),primitives[i].vertices.end());for(int index:{0,1,2,0,2,3})indices.push_back(base+index);}
        const auto geometry=backend_.CompileGeometry({vertices.data(),vertices.size()},{indices.data(),indices.size()});
        try{backend_.RenderGeometry(geometry,{0,0},primitives[begin].texture);}catch(...){backend_.ReleaseGeometry(geometry);throw;}
        backend_.ReleaseGeometry(geometry);++statistics.batches;statistics.vertices+=vertices.size();statistics.indices+=indices.size();begin=end;
    }return statistics;
}
std::optional<std::array<float,2>> Renderer2D::projectLabel(std::array<float,4> clip,VkExtent2D viewport){
    for(float value:clip)if(!std::isfinite(value))return {};
    if(!viewport.width||!viewport.height||clip[3]<=0||clip[2]<0||clip[2]>clip[3]||std::abs(clip[0])>clip[3]||std::abs(clip[1])>clip[3])return {};
    return std::array<float,2>{(clip[0]/clip[3]+1)*.5F*viewport.width,(clip[1]/clip[3]+1)*.5F*viewport.height};
}
}
