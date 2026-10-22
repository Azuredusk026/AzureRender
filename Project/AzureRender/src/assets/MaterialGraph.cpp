#include "assets/MaterialGraph.hpp"
#include <cmath>
#include <algorithm>
#include <set>
namespace azurerender {
using Json=nlohmann::json;
namespace {
bool validId(const std::string& id){return !id.empty()&&id.size()<=64&&id.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-")==std::string::npos;}
}
MaterialGraph MaterialGraph::fromMaterial(const MaterialData& material){
    auto copy=material;copy.authoringGraph=nullptr;copy.validate();
    MaterialGraph graph;graph.document_={{"schemaVersion",1},{"typeId",copy.typeId},{"nodes",Json::array({{{"id","output"},{"kind","output"},{"position",{400,100}}}})},{"links",Json::array()}};
    if(copy.parameterOverride){graph.document_["schemaVersion"]=2;graph.document_["mode"]="parameters";}
    if(copy.typeId=="azure.profile"){graph.document_["schemaVersion"]=3;graph.document_["surface"]=copy.surface;graph.document_["images"]=copy.textures;return graph;}
    for(const auto& field:copy.parameters.items())graph.setField(field.key(),field.value());
    for(const auto& field:copy.textures.items())graph.setField("textures."+field.key(),field.value());
    return graph;
}
MaterialGraph MaterialGraph::parse(const Json& document){MaterialGraph graph;graph.document_=document;static_cast<void>(graph.compile());return graph;}
Json MaterialGraph::encode()const{static_cast<void>(compile());return document_;}
MaterialData MaterialGraph::compile()const{
    const auto& graph=document_;
    if(!graph.is_object()||!graph.at("schemaVersion").is_number_integer())throw std::invalid_argument("Invalid material graph version");
    const bool patch=graph.at("schemaVersion")==2;
    if(graph.at("schemaVersion")==3){
        if(graph.size()!=6||graph.at("typeId")!="azure.profile"||!graph.at("links").is_array()||graph.at("nodes")!=Json::array({{{"id","output"},{"kind","output"},{"position",graph.at("nodes").at(0).at("position")}}})||!graph.at("links").empty())throw std::invalid_argument("Invalid surface graph");
        const auto& position=graph.at("nodes").at(0).at("position");
        if(!position.is_array()||position.size()!=2)throw std::invalid_argument("Invalid surface graph position");
        for(const auto& coordinate:position)if(!coordinate.is_number()||!std::isfinite(coordinate.get<double>())||std::abs(coordinate.get<double>())>1000000)throw std::invalid_argument("Invalid surface graph coordinate");
        auto material=MaterialData::defaults("azure.profile");material.surface=graph.at("surface");material.textures=graph.at("images");material.validate();return material;
    }
    if(graph.size()!=(patch?5:4)||(!patch&&graph.at("schemaVersion")!=1)
        ||(patch&&graph.at("mode")!="parameters")||!graph.at("nodes").is_array()||!graph.at("links").is_array()||graph.at("nodes").size()>128||graph.at("links").size()>64)throw std::invalid_argument("Invalid material graph schema or budget");
    auto material=MaterialData::defaults(graph.at("typeId").get<std::string>());
    if(patch){material.parameterOverride=true;material.parameters=Json::object();}
    std::map<std::string,Json> nodes;unsigned outputs=0;
    for(const auto& node:graph.at("nodes")){
        const auto id=node.at("id").get<std::string>(),kind=node.at("kind").get<std::string>();
        if(!validId(id)||!nodes.emplace(id,node).second||!node.at("position").is_array()||node.at("position").size()!=2)throw std::invalid_argument("Invalid graph node identity or layout");
        for(const auto& coordinate:node.at("position"))if(!coordinate.is_number()||!std::isfinite(coordinate.get<double>())||std::abs(coordinate.get<double>())>1000000)throw std::invalid_argument("Invalid graph layout coordinate");
        if(kind=="output"){if(id!="output"||node.size()!=3)throw std::invalid_argument("Invalid material output");++outputs;}
        else if((kind!="value"&&kind!="texture")||node.size()!=4||!node.contains("value"))throw std::invalid_argument("Invalid graph node kind");
    }
    if(outputs!=1)throw std::invalid_argument("One material output is required");
    std::set<std::string> targets;
    for(const auto& link:graph.at("links")){
        const auto slot=link.at("slot").get<std::string>(),source=link.at("source").get<std::string>();
        if(link.size()!=3||link.at("target")!="output"||!nodes.count(source)||source=="output"||!targets.insert(slot).second)throw std::invalid_argument("Invalid graph connection");
        const auto& node=nodes.at(source);const bool texture=slot.rfind("textures.",0)==0;
        if(node.at("kind")!=(texture?"texture":"value"))throw std::invalid_argument("Graph port type mismatch");
        if(texture)material.textures[slot.substr(9)]=node.at("value");
        else{if(!MaterialTypeRegistry::builtins().type(material.typeId).parameters.contains(slot))throw std::invalid_argument("Unknown material graph port");material.parameters[slot]=node.at("value");}
    }
    material.validate();return material;
}
void MaterialGraph::setField(const std::string& field,const Json& value){
    auto candidate=document_;const bool texture=field.rfind("textures.",0)==0;const auto id="field_"+(texture?field.substr(9):field);
    const auto& type=MaterialTypeRegistry::builtins().type(candidate.at("typeId").get<std::string>());
    if(texture?!type.textures.contains(field.substr(9)):!type.parameters.contains(field))throw std::invalid_argument("Unknown material graph field");
    auto& links=candidate.at("links");links.erase(std::remove_if(links.begin(),links.end(),[&](const auto& link){return link.at("slot")==field;}),links.end());
    auto& nodes=candidate.at("nodes");auto found=std::find_if(nodes.begin(),nodes.end(),[&](const auto& node){return node.at("id")==id;});
    if(value.is_null()&&(texture||candidate.at("schemaVersion")==2)){if(found!=nodes.end())nodes.erase(found);}
    else{
        if(found==nodes.end()){nodes.push_back({{"id",id},{"kind",texture?"texture":"value"},{"position",{20,static_cast<int>(nodes.size())*90}},{"value",value}});}
        else (*found)["value"]=value;
        links.push_back({{"source",id},{"target","output"},{"slot",field}});
    }
    static_cast<void>(parse(candidate));document_=std::move(candidate);
}
}
