#include "runtime/RuntimeRenderSnapshotCache.hpp"
#include <iostream>
#include <stdexcept>
using namespace azurerender;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void equal(const scene::SceneDescription& a,const scene::SceneDescription& b){
    require(a.nodes.size()==b.nodes.size()&&a.resources.size()==b.resources.size()&&a.lights.size()==b.lights.size(),"Prepared membership differs");
    for(std::size_t i=0;i<a.nodes.size();++i){const auto& x=a.nodes[i];const auto& y=b.nodes[i];
        require(x.id==y.id&&x.resourceId==y.resourceId&&x.parentId==y.parentId&&x.translation==y.translation&&x.rotation==y.rotation&&x.scale==y.scale&&x.visible==y.visible,"Prepared node differs");}
    for(std::size_t i=0;i<a.lights.size();++i){const auto& x=a.lights[i];const auto& y=b.lights[i];
        require(x.id==y.id&&x.nodeId==y.nodeId&&x.position==y.position&&x.shape.direction==y.shape.direction,"World-space light differs");}
}
}
int main(){try{
    SceneDocument doc;doc.resources.push_back({"mesh","model","mesh.gltf"});
    SceneNode parent;parent.id="parent";parent.translation={1,2,3};
    SceneNode child;child.id="child";child.parentId="parent";child.resourceId="mesh";child.translation={4,0,0};doc.nodes={parent,child};
    SceneLight light;light.id="light";light.nodeId="child";light.shape.direction={1,0,0};doc.lights.push_back(light);
    RuntimeRenderSnapshotCache cache;auto source=std::make_shared<const int>(1);unsigned preparations=0;
    const auto prepare=[&](const SceneDocument& value){++preparations;auto result=value.renderDescription();for(auto& resource:result.resources)resource.path="resolved/"+resource.path;return result;};
    auto first=cache.prepare(doc,source,prepare);equal(doc.renderDescription(),*first);require(first->resources[0].path=="resolved/mesh.gltf","Resource preparation must be preserved");
    doc.nodes[0].translation={7,8,9};doc.nodes[0].rotation={0,90,0};doc.nodes[1].visible=false;
    auto second=cache.prepare(doc,source,prepare);equal(doc.renderDescription(),*second);
    require(preparations==1&&first->nodes[0].translation==std::array<float,3>{1,2,3},"Retained frame inputs must stay immutable");
    require(first->lights[0].position!=second->lights[0].position,"Light motion positive control missing");
    std::vector<std::shared_ptr<const scene::SceneDescription>> retained{first,second};
    for(unsigned i=0;i<6;++i){doc.nodes[0].translation[0]+=1;retained.push_back(cache.prepare(doc,source,prepare));}
    require(cache.pooledFrameCount()<=3&&preparations==1,"Retained frames must not grow the owned reuse pool");
    source=std::make_shared<const int>(2);doc.resources[0].path="replacement.gltf";
    auto changed=cache.prepare(doc,source,prepare);require(preparations==2&&changed->resources[0].path=="resolved/replacement.gltf","Source renewal must prepare resources");
    doc.nodes.erase(doc.nodes.begin());doc.nodes[0].parentId.clear();
    auto pruned=cache.prepare(doc,source,prepare);equal(doc.renderDescription(),*pruned);require(preparations==3,"Node removal must renew the layout");
    bool rejected=false;try{cache.prepare(doc,{},prepare);}catch(const std::invalid_argument&){rejected=true;}require(rejected,"Missing source ownership must be rejected");
    cache.clear();cache.prepare(doc,source,prepare);require(preparations==4,"Explicit clear must renew the prepared description");
    require(first->nodes.size()==2&&first->resources[0].path=="resolved/mesh.gltf","Source renewal must preserve externally retained frames");
    std::cout<<"Runtime render snapshot cache contracts passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
