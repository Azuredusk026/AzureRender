#include "animation/PresentedPose.hpp"
#include "render/pathtracing/TraceScene.hpp"
#include "assets/AssetDeformation.hpp"
#include "animation/ExpressionPose.hpp"
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
Vector3 direction(const Matrix4& m,Vector3 v){return {m[0]*v[0]+m[4]*v[1]+m[8]*v[2],m[1]*v[0]+m[5]*v[1]+m[9]*v[2],m[2]*v[0]+m[6]*v[1]+m[10]*v[2]};}
float determinant(const Matrix4& m){return dot(Vector3{m[0],m[1],m[2]},cross(Vector3{m[4],m[5],m[6]},Vector3{m[8],m[9],m[10]}));}
Vector3 normalDirection(const Matrix4& m,Vector3 n){
 const Vector3 a{m[0],m[1],m[2]},b{m[4],m[5],m[6]},c{m[8],m[9],m[10]};
 const auto value=addVectors(addVectors(scaleVector(cross(b,c),n[0]),scaleVector(cross(c,a),n[1])),scaleVector(cross(a,b),n[2]));
 const float d=dot(a,cross(b,c));if(std::abs(d)<1e-12F)throw std::invalid_argument("Singular tracing normal transform");
 return normalize(scaleVector(value,1/d));
}

}
std::array<std::uint32_t,4> appendTexture(std::vector<std::uint32_t>& pixels,const std::vector<std::uint8_t>& rgba,unsigned width,unsigned height,bool srgb){
    if(!width||!height||rgba.size()<static_cast<std::size_t>(width)*height*4)throw std::invalid_argument("Invalid tracing texture");
    if(pixels.size()+static_cast<std::size_t>(width)*height>32*1024*1024)throw std::invalid_argument("Path tracing texture atlas exceeds 128 MiB");
    const auto start=static_cast<unsigned>(pixels.size());
    for(std::size_t i=0;i<static_cast<std::size_t>(width)*height;++i)pixels.push_back(static_cast<unsigned>(rgba[i*4])|(static_cast<unsigned>(rgba[i*4+1])<<8)|(static_cast<unsigned>(rgba[i*4+2])<<16)|(static_cast<unsigned>(rgba[i*4+3])<<24));
    return {start,width,height,srgb?1U:0U};
}
std::vector<float> environmentDistribution(unsigned width,unsigned height,const std::vector<std::uint16_t>& rgba,unsigned columns,unsigned rows){
 if(!width||!height||!columns||!rows||rgba.size()!=static_cast<std::size_t>(width)*height*4)throw std::invalid_argument("Invalid environment distribution");
 const auto half=[](std::uint16_t h){const unsigned e=(h>>10)&31,m=h&1023;return (h&32768?-1.F:1.F)*(e?std::ldexp(1.F+static_cast<float>(m)/1024,static_cast<int>(e)-15):std::ldexp(static_cast<float>(m),-24));};
 std::vector<float> result(columns*rows*2);double total=0;
 for(unsigned y=0;y<rows;++y)for(unsigned x=0;x<columns;++x){const auto pixel=(static_cast<std::size_t>(std::min(height-1,(2*y+1)*height/(2*rows)))*width+std::min(width-1,(2*x+1)*width/(2*columns)))*4;
 const float luminance=std::max(.001F,.2126F*half(rgba[pixel])+.7152F*half(rgba[pixel+1])+.0722F*half(rgba[pixel+2]));
 const float weight=luminance*(std::cos(3.14159265359F*static_cast<float>(y)/static_cast<float>(rows))-std::cos(3.14159265359F*static_cast<float>(y+1)/static_cast<float>(rows)));result[columns*rows+y*columns+x]=weight;total+=weight;}
 float cumulative=0;for(unsigned i=0;i<columns*rows;++i){result[columns*rows+i]=static_cast<float>(result[columns*rows+i]/total);cumulative+=result[columns*rows+i];result[i]=cumulative;}result[columns*rows-1]=1;return result;
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
            pose=samplePresentedAnimationPose(asset,*animation);morph=animation->morph;
        }
        if(animation!=animations.end()) {
            if(animation->expression)pose=applyExpressionPose(asset,pose,*animation->expression,animation->expressionWeight);
            if(!asset.morphRanges.empty()) {
                for(auto& values:pose.morphWeights)for(unsigned i=0;i<std::min<std::size_t>(2,values.size());++i)if(morph[i]!=0)values[i]=morph[i];
                applyMorphOverrides(asset,pose,animation->morphOverrides);
            }
        }
        const auto& world=worlds[nodeIndex];
        for(const auto& primitive:asset.primitives){
            for(unsigned i=0;i+2<primitive.indexCount;i+=3){
                const auto a=asset.indices.at(primitive.firstIndex+i);auto b=asset.indices.at(primitive.firstIndex+i+1),c=asset.indices.at(primitive.firstIndex+i+2);if(determinant(world)<0)std::swap(b,c);
                const auto position=[&](unsigned v){return transformPosition(world,deformedVertexPosition(asset,v,&pose,morph));};
                const auto pa=position(a),pb=position(b),pc=position(c);const auto e1=subtract(pb,pa),e2=subtract(pc,pa);
                if(vectorLength(cross(e1,e2))<1e-10F)continue;
                const auto& ua=asset.vertices.at(a).texcoord;const auto& ub=asset.vertices.at(b).texcoord;const auto& uc=asset.vertices.at(c).texcoord;
                const auto& material=asset.materials.at(primitive.materialIndex);
                Triangle triangle{packed(pa),packed(e1),packed(e2),{ua[0],ua[1],ub[0],ub[1]},{uc[0],uc[1],static_cast<float>(mesh.materialBase+primitive.materialIndex),material.emissiveStrength>0?1.F:0.F}};
                const auto frame=[&](unsigned v){
                    const auto& vertex=asset.vertices.at(v);
                    auto normal=deformedVertexDirection(asset,v,&pose,1);
                    auto tangent=deformedVertexDirection(asset,v,&pose,2);
                    normal=normalDirection(world,normal);tangent=direction(world,tangent);tangent=subtract(tangent,scaleVector(normal,dot(normal,tangent)));
                    if(vectorLength(tangent)<1e-8F)tangent=cross(std::abs(normal[1])<.99F?Vector3{0,1,0}:Vector3{1,0,0},normal);
                    return std::pair<Vec4,Vec4>{packed(normal),packed(normalize(tangent),vertex.tangent[3]*(determinant(world)<0?-1.F:1.F))};
                };
                const auto fa=frame(a),fb=frame(b),fc=frame(c);triangle.n0=fa.first;triangle.n1=fb.first;triangle.n2=fc.first;triangle.t0=fa.second;triangle.t1=fb.second;triangle.t2=fc.second;result.triangles.push_back(triangle);
            }
        }
    }
    buildBvh(result);
    for(std::size_t i=0;i<result.triangles.size();++i)if(result.triangles[i].uv2material[3]>0)result.lights.push_back({{static_cast<float>(i),0,0,0},{}});
    for(const auto& light:scene.lights)if(light.enabled&&light.intensity>0){
        light.shape.validate();
        if(light.shape.characterResponse.isColorBlend())continue;
        result.lights.push_back({{light.position[0],light.position[1],light.position[2],1},
            {light.color[0]*light.intensity,light.color[1]*light.intensity,light.color[2]*light.intensity,light.radius},
            {light.shape.direction[0],light.shape.direction[1],light.shape.direction[2],std::cos(light.shape.outerCone)},
            {std::cos(light.shape.innerCone),static_cast<float>(light.shape.type),
                static_cast<float>(light.shape.falloff),light.shape.falloffExponent},
            light.shape.influenceBox.worldToBox,
            {light.shape.influenceBox.enabled?1.F:0.F,light.shape.influenceBox.falloffThreshold,light.shape.specularScale,static_cast<float>(light.shape.receivers.mask())}});
    }
    return result;
}
std::uint64_t sceneFingerprint(const scene::SceneDescription& scene,const std::vector<NodeAnimationFrame>& animations){
    std::uint64_t hash=1469598103934665603ULL;
    const auto bytes=[&](const auto& value){const auto* p=reinterpret_cast<const unsigned char*>(&value);for(std::size_t i=0;i<sizeof(value);++i){hash^=p[i];hash*=1099511628211ULL;}};
    const auto text=[&](const std::string& value){for(unsigned char c:value){hash^=c;hash*=1099511628211ULL;}hash^=255;hash*=1099511628211ULL;};
    for(const auto& n:scene.nodes){text(n.id);text(n.resourceId);text(n.parentId);bytes(n.translation);bytes(n.rotation);bytes(n.scale);bytes(n.visible);}
    for(const auto& l:scene.lights){text(l.id);bytes(l.position);bytes(l.color);bytes(l.intensity);bytes(l.radius);bytes(l.enabled);bytes(l.shape.type);bytes(l.shape.direction);bytes(l.shape.innerCone);bytes(l.shape.outerCone);bytes(l.shape.falloff);bytes(l.shape.falloffExponent);bytes(l.shape.specularScale);}
    for(const auto& light:scene.lights){bytes(light.shape.influenceBox.enabled);bytes(light.shape.influenceBox.worldToBox);bytes(light.shape.influenceBox.falloffThreshold);}
    for(const auto& light:scene.lights){bytes(light.shape.receivers.surfaces);bytes(light.shape.receivers.characters);}
    for(const auto& light:scene.lights){bytes(light.shape.characterResponse.isColorBlend());}
    for(const auto& a:animations){text(a.node);bytes(a.clip);bytes(a.time);bytes(a.loop);bytes(a.previousClip);bytes(a.previousTime);bytes(a.blend);bytes(a.previousLoop);bytes(a.morph);bytes(a.expressionWeight);
        for(const auto& sample:a.samples){bytes(sample.clip);bytes(sample.time);bytes(sample.weight);bytes(sample.loop);bytes(sample.duration);}bytes(a.samples.size());
        for(float w:a.morphOverrides)bytes(w);
        bytes(a.morphOverrides.size());
        if(a.expression)text(expressionPoseToJson(*a.expression).dump());
    }
    return hash;
}
}
