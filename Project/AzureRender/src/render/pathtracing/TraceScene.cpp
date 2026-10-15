#include "render/pathtracing/TraceScene.hpp"
#include "assets/AssetDeformation.hpp"
#include "render/LinearMath.hpp"
#include <algorithm>
#include <cstring>
#include <numeric>
#include <limits>
namespace azurerender::tracing {
using namespace internal;
namespace {
Vec4 packed(Vector3 p,float w=0){return {p[0],p[1],p[2],w};}
Vector3 xyz(const Vec4& p){return {p[0],p[1],p[2]};}
}
std::array<std::uint32_t,4> appendTexture(std::vector<std::uint32_t>& pixels,const std::vector<std::uint8_t>& rgba,unsigned width,unsigned height,bool srgb){
    if(!width||!height||rgba.size()<static_cast<std::size_t>(width)*height*4)throw std::invalid_argument("Invalid tracing texture");
    if(pixels.size()+static_cast<std::size_t>(width)*height>32*1024*1024)throw std::invalid_argument("Path tracing texture atlas exceeds 128 MiB");
    const auto start=static_cast<unsigned>(pixels.size());
    for(std::size_t i=0;i<static_cast<std::size_t>(width)*height;++i)pixels.push_back(static_cast<unsigned>(rgba[i*4])|(static_cast<unsigned>(rgba[i*4+1])<<8)|(static_cast<unsigned>(rgba[i*4+2])<<16)|(static_cast<unsigned>(rgba[i*4+3])<<24));
    return {start,width,height,srgb?1U:0U};
}
void buildBvh(Geometry& geometry){
    geometry.nodes.clear();if(geometry.triangles.empty())return;
    if(geometry.triangles.size()>1000000)throw std::invalid_argument("Path tracing scene exceeds one million triangles");
    const auto build=[&](std::uint32_t first,std::uint32_t count,auto&& self)->std::uint32_t{
        const auto index=static_cast<std::uint32_t>(geometry.nodes.size());geometry.nodes.emplace_back();
        Vector3 low{1e30F,1e30F,1e30F},high{-1e30F,-1e30F,-1e30F};
        for(unsigned i=first;i<first+count;++i){const auto& t=geometry.triangles[i];for(const auto& point:{xyz(t.p0),addVectors(xyz(t.p0),xyz(t.e1)),addVectors(xyz(t.p0),xyz(t.e2))})for(unsigned a=0;a<3;++a){if(!std::isfinite(point[a]))throw std::invalid_argument("Nonfinite tracing geometry");low[a]=std::min(low[a],point[a]);high[a]=std::max(high[a],point[a]);}}
        geometry.nodes[index].minimum=packed(low);geometry.nodes[index].maximum=packed(high);
        if(count<=4){geometry.nodes[index].links={first,count,0,0};return index;}
        unsigned axis=0;for(unsigned a=1;a<3;++a)if(high[a]-low[a]>high[axis]-low[axis])axis=a;
        const auto middle=first+count/2;
        std::nth_element(geometry.triangles.begin()+first,geometry.triangles.begin()+middle,geometry.triangles.begin()+first+count,[axis](const auto& a,const auto& b){return a.p0[axis]+(a.e1[axis]+a.e2[axis])/3.F<b.p0[axis]+(b.e1[axis]+b.e2[axis])/3.F;});
        const auto left=self(first,count/2,self),right=self(middle,count-count/2,self);
        geometry.nodes[index].links={left,0,right,0};return index;
    };build(0,static_cast<unsigned>(geometry.triangles.size()),build);
}
Geometry buildGeometry(const scene::SceneDescription& scene,const Meshes& meshes,const std::vector<NodeAnimationFrame>& animations){
    Geometry result;const auto worlds=scene::resolveNodeWorldTransforms(scene);
    for(std::size_t nodeIndex=0;nodeIndex<scene.nodes.size();++nodeIndex){const auto& node=scene.nodes[nodeIndex];if(!node.visible)continue;
        const auto found=meshes.find(node.resourceId);if(found==meshes.end())continue;const auto& mesh=found->second;const auto& asset=mesh.asset;
        auto pose=bindAnimationPose(asset);std::array<float,2> morph{};
        const auto animation=std::find_if(animations.begin(),animations.end(),[&](const auto& a){return a.node==node.id;});
        if(animation!=animations.end()){
            pose=sampleAnimationPose(asset,animation->clip,static_cast<float>(animation->time),animation->loop);morph=animation->morph;
            if(animation->blend<1)pose=blendAnimationPoses(asset,sampleAnimationPose(asset,animation->previousClip,static_cast<float>(animation->previousTime),animation->previousLoop),pose,animation->blend);
        }
        const auto& world=worlds[nodeIndex];
        for(const auto& primitive:asset.primitives){
            for(unsigned i=0;i+2<primitive.indexCount;i+=3){
                const auto a=asset.indices.at(primitive.firstIndex+i),b=asset.indices.at(primitive.firstIndex+i+1),c=asset.indices.at(primitive.firstIndex+i+2);
                const auto position=[&](unsigned v){return transformPosition(world,deformedVertexPosition(asset,v,asset.hasSkin?&pose:nullptr,morph));};
                const auto pa=position(a),pb=position(b),pc=position(c);const auto e1=subtract(pb,pa),e2=subtract(pc,pa);
                if(vectorLength(cross(e1,e2))<1e-10F)continue;
                const auto& ua=asset.vertices.at(a).texcoord;const auto& ub=asset.vertices.at(b).texcoord;const auto& uc=asset.vertices.at(c).texcoord;
                const auto& material=asset.materials.at(primitive.materialIndex);
                result.triangles.push_back({packed(pa),packed(e1),packed(e2),{ua[0],ua[1],ub[0],ub[1]},{uc[0],uc[1],static_cast<float>(mesh.materialBase+primitive.materialIndex),material.emissiveStrength>0?1.F:0.F}});
            }
        }
    }
    buildBvh(result);
    for(std::size_t i=0;i<result.triangles.size();++i)if(result.triangles[i].uv2material[3]>0)result.lights.push_back({{static_cast<float>(i),0,0,0},{}});
    for(const auto& light:scene.lights)if(light.enabled&&light.intensity>0)result.lights.push_back({{light.position[0],light.position[1],light.position[2],1},{light.color[0]*light.intensity,light.color[1]*light.intensity,light.color[2]*light.intensity,light.radius}});
    return result;
}
std::uint64_t sceneFingerprint(const scene::SceneDescription& scene,const std::vector<NodeAnimationFrame>& animations){
    std::uint64_t hash=1469598103934665603ULL;
    const auto bytes=[&](const auto& value){const auto* p=reinterpret_cast<const unsigned char*>(&value);for(std::size_t i=0;i<sizeof(value);++i){hash^=p[i];hash*=1099511628211ULL;}};
    const auto text=[&](const std::string& value){for(unsigned char c:value){hash^=c;hash*=1099511628211ULL;}hash^=255;hash*=1099511628211ULL;};
    for(const auto& n:scene.nodes){text(n.id);text(n.resourceId);text(n.parentId);bytes(n.translation);bytes(n.rotation);bytes(n.scale);bytes(n.visible);}
    for(const auto& l:scene.lights){text(l.id);bytes(l.position);bytes(l.color);bytes(l.intensity);bytes(l.radius);bytes(l.enabled);}
    for(const auto& a:animations){text(a.node);bytes(a.clip);bytes(a.time);bytes(a.loop);bytes(a.previousClip);bytes(a.previousTime);bytes(a.blend);bytes(a.previousLoop);bytes(a.morph);}
    return hash;
}
}
