#include "render/ReferenceCameraViewJson.hpp"
#include <nlohmann/json.hpp>
namespace azurerender {
namespace {
using Json=nlohmann::json;
void fields(const Json& j,std::initializer_list<const char*> names,std::initializer_list<const char*> optional={}){
    if(!j.is_object())throw std::invalid_argument("Unexpected reference camera fields");
    auto known=names.size();
    for(auto name:optional)if(j.contains(name))++known;
    if(j.size()!=known)throw std::invalid_argument("Unexpected reference camera fields");
    for(auto name:names)if(!j.contains(name))throw std::invalid_argument("Missing reference camera field");
}
void version(const Json& j){if(!j.at("schemaVersion").is_number_integer()||j.at("schemaVersion")!=1)throw std::invalid_argument("Unsupported reference camera JSON version");}
float number(const Json& j){if(!j.is_number())throw std::invalid_argument("Reference camera requires numeric fields");const auto d=j.get<double>();const auto value=static_cast<float>(d);if(!std::isfinite(d)||!std::isfinite(value))throw std::invalid_argument("Reference camera value exceeds finite binary32");return value;}
template<std::size_t N> std::array<float,N> vector(const Json& j){if(!j.is_array()||j.size()!=N)throw std::invalid_argument("Invalid reference camera vector size");std::array<float,N> result{};for(unsigned i=0;i<N;++i)result[i]=number(j[i]);return result;}
}
void to_json(nlohmann::json& j,const ReferenceCameraView& v){
    (void)PreparedReferenceCameraView(v);
    j={{"schemaVersion",1},{"frame",{{"schemaVersion",1},{"axes",{v.frame.parameters.axes[0],v.frame.parameters.axes[1],v.frame.parameters.axes[2]}},{"offset",{v.frame.parameters.offset[0],v.frame.parameters.offset[1],v.frame.parameters.offset[2]}}}},
        {"projection",{{"schemaVersion",1},{"columns",v.projection.parameters.columns},{"jitter",{v.projection.parameters.jitter[0],v.projection.parameters.jitter[1]}}}},
        {"extent",v.extent},{"depthConvention",v.depthConvention==ReferenceDepthConvention::Forward?"forward":"reversed"},{"flipNdcY",v.flipNdcY}};
    if(v.produceSceneDepth)j["produceSceneDepth"]=true;
    if(v.produceSceneVisibilityInputs)j["produceSceneVisibilityInputs"]=true;
}
void from_json(const nlohmann::json& j,ReferenceCameraView& value){
    fields(j,{"schemaVersion","frame","projection","extent","depthConvention","flipNdcY"},
        {"produceSceneDepth","produceSceneVisibilityInputs"});
    version(j);ReferenceCameraView v;
    const auto& frame=j.at("frame");fields(frame,{"schemaVersion","axes","offset"});version(frame);
    const auto& axes=frame.at("axes");if(!axes.is_array()||axes.size()!=3)throw std::invalid_argument("Invalid reference camera axis count");
    for(unsigned i=0;i<3;++i){if(!axes[i].is_number_integer()||axes[i]<-3||axes[i]>3||axes[i]==0)throw std::invalid_argument("Invalid reference camera axis");v.frame.parameters.axes[i]=axes[i].get<int>();}
    const auto offset=vector<3>(frame.at("offset"));for(unsigned i=0;i<3;++i)v.frame.parameters.offset[i]=offset[i];
    const auto& projection=j.at("projection");fields(projection,{"schemaVersion","columns","jitter"});version(projection);
    const auto& columns=projection.at("columns");if(!columns.is_array()||columns.size()!=4)throw std::invalid_argument("Invalid reference camera matrix column count");
    for(unsigned i=0;i<4;++i)v.projection.parameters.columns[i]=vector<4>(columns[i]);const auto jitter=vector<2>(projection.at("jitter"));v.projection.parameters.jitter={jitter[0],jitter[1],0,0};
    const auto& extent=j.at("extent");if(!extent.is_array()||extent.size()!=2)throw std::invalid_argument("Invalid reference camera viewport size");
    for(unsigned i=0;i<2;++i){if(!extent[i].is_number_integer()||extent[i]<1||extent[i]>8192)throw std::invalid_argument("Reference camera extent must be1..8192");v.extent[i]=extent[i].get<unsigned>();}
    if(j.at("depthConvention")=="forward")v.depthConvention=ReferenceDepthConvention::Forward;
    else if(j.at("depthConvention")=="reversed")v.depthConvention=ReferenceDepthConvention::Reversed;
    else throw std::invalid_argument("Unknown reference camera depth convention");
    if(!j.at("flipNdcY").is_boolean())throw std::invalid_argument("Reference camera flip must be boolean");v.flipNdcY=j.at("flipNdcY").get<bool>();
    if(j.contains("produceSceneDepth")){
        if(!j.at("produceSceneDepth").is_boolean())throw std::invalid_argument("Reference camera produceSceneDepth must be boolean");
        v.produceSceneDepth=j.at("produceSceneDepth").get<bool>();
    }
    if(j.contains("produceSceneVisibilityInputs")){
        if(!j.at("produceSceneVisibilityInputs").is_boolean())throw std::invalid_argument("Scene visibility input flag must be boolean");
        v.produceSceneVisibilityInputs=j.at("produceSceneVisibilityInputs").get<bool>();
    }
    (void)PreparedReferenceCameraView(v);value=std::move(v);
}
}
