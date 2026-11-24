#include "runtime/SceneDocument.hpp"
#include "scene/SceneDescription.hpp"
#include <iostream>
#include <stdexcept>
using namespace azurerender;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
SceneNode node(std::string id,bool overrideMaterial,const char* document="red.azurematerial"){
    SceneNode result;result.id=std::move(id);result.resourceId="mesh";
    if(overrideMaterial){result.materialSlots["surface"]="material-id";result.resolvedMaterials["surface"]={document,"fingerprint",{}};}
    return result;
}
}
int main(){try{
    SceneDocument document;document.resources={{"mesh","model","C:/models/model.glb"}};
    document.nodes={node("first",true)};
    auto snapshot=document.renderDescription();
    require(snapshot.resources.size()==1,"A fully overridden mesh must have one consumed resource, without an unused base copy");
    require(snapshot.resources[0].id==snapshot.nodes[0].resourceId&&!snapshot.resources[0].materials.empty(),"The consumed resource must retain its material binding and node identity");
    require(document.resources.size()==1&&document.resources[0].id=="mesh"&&document.nodes[0].resourceId=="mesh","Snapshot preparation must preserve the source document");
    document.nodes.push_back(node("same",true));snapshot=document.renderDescription();
    require(snapshot.resources.size()==2&&scene::ResourceLayout(snapshot).paths.size()==1,"Identical overridden nodes must retain aliases while sharing their mesh layout");
    document.nodes[1]=node("different",true,"blue.azurematerial");snapshot=document.renderDescription();
    require(snapshot.resources.size()==2&&scene::ResourceLayout(snapshot).paths.size()==2,"Different material bindings must retain distinct mesh variants");
    document.nodes.push_back(node("ordinary",false));snapshot=document.renderDescription();
    require(snapshot.resources.size()==3&&snapshot.resources[0].id=="mesh","An ordinary instance must retain the base resource and stable ordering");
    document.nodes.back().visible=false;snapshot=document.renderDescription();
    require(snapshot.resources.size()==3,"Invisible ordinary instances must retain their resource for visibility changes");
    document.nodes={node("ordinary",false)};document.resources.push_back({"reserved","model","C:/models/reserved.glb"});
    snapshot=document.renderDescription();require(snapshot.resources.size()==2&&snapshot.resources[0].id=="mesh"&&snapshot.resources[1].id=="reserved","Documents without material variants retain their declared resource layout");
    document.nodes={node("overridden",true)};snapshot=document.renderDescription();
    require(snapshot.resources.size()==2&&snapshot.resources[0].path=="C:/models/model.glb"&&snapshot.resources[1].id=="reserved","The first consumed variant retains model ordering while unrelated resources retain their declared state");
    document.nodes[0].resolvedMaterials.clear();bool rejected=false;
    try{static_cast<void>(document.renderDescription());}catch(const std::invalid_argument&){rejected=true;}
    require(rejected,"Unresolved material bindings must reject before producing a snapshot");
    SceneDocument several;several.resources={{"mesh","model","C:/models/model.glb"},{"second","model","C:/models/second.glb"}};
    several.nodes={node("first",true),node("second-node",true)};several.nodes[1].resourceId="second";
    snapshot=several.renderDescription();
    require(snapshot.resources.size()==2&&snapshot.resources[0].path=="C:/models/model.glb"&&snapshot.resources[1].path=="C:/models/second.glb","Multiple fully overridden models preserve their source geometry order");
    SceneDocument ambiguous;ambiguous.resources={{"mesh","model","C:/models/model.glb"},{"mesh::materials::first","model","C:/models/authored.glb"}};
    ambiguous.nodes={node("first",true)};rejected=false;
    try{static_cast<void>(ambiguous.renderDescription());}catch(const std::invalid_argument&){rejected=true;}
    require(rejected,"Generated material identities must reject collisions with authored resources");
    std::cout<<"Material variant ownership, aliasing and source-document isolation passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
