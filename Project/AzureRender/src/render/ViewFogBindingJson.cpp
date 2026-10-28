#include "render/ViewFogBindingJson.hpp"
#include <nlohmann/json.hpp>
#include <cmath>
#include <limits>
#include <stdexcept>
namespace azurerender {
namespace {
using Json=nlohmann::json;
void fields(const Json& j,std::initializer_list<const char*> names){
    if(!j.is_object()||j.size()!=names.size())throw std::invalid_argument("Invalid view fog fields");
    for(const auto* name:names)if(!j.contains(name))throw std::invalid_argument(std::string("Missing view fog field: ")+name);
}
void version(const Json& j){if(!j.at("schemaVersion").is_number_integer()||j.at("schemaVersion")!=1)throw std::invalid_argument("Unsupported view fog schema version");}
float number(const Json& j){if(!j.is_number()||!std::isfinite(j.get<double>())||std::abs(j.get<double>())>std::numeric_limits<float>::max())throw std::invalid_argument("Invalid view fog number");return j.get<float>();}
std::uint32_t integer(const Json& j,std::uint32_t maximum){if(!j.is_number_integer()||j.get<double>()<0||j.get<double>()>maximum)throw std::invalid_argument("Invalid view fog integer");return j.get<std::uint32_t>();}
template<std::size_t N> std::array<float,N> vector(const Json& j){if(!j.is_array()||j.size()!=N)throw std::invalid_argument("Invalid view fog vector");std::array<float,N> result{};for(unsigned i=0;i<N;++i)result[i]=number(j[i]);return result;}
Json encodeView(const CameraViewDescriptor& v){return {{"projection",v.projection==CameraProjectionKind::Perspective?"perspective":"orthographic"},
    {"position",v.position},{"target",v.target},{"up",v.up},{"fieldOfViewRadians",v.fov},{"orthographicHeight",v.orthoHeight},{"nearPlane",v.nearPlane},{"farPlane",v.farPlane},{"projectionOffset",{v.projectionOffsetX,v.projectionOffsetY}}};}
CameraViewDescriptor decodeView(const Json& j){
    fields(j,{"projection","position","target","up","fieldOfViewRadians","orthographicHeight","nearPlane","farPlane","projectionOffset"});CameraViewDescriptor result;
    if(j.at("projection")=="orthographic")result.projection=CameraProjectionKind::Orthographic;else if(j.at("projection")!="perspective")throw std::invalid_argument("Invalid view fog camera projection");
    result.position=vector<3>(j.at("position"));result.target=vector<3>(j.at("target"));result.up=vector<3>(j.at("up"));result.fov=number(j.at("fieldOfViewRadians"));result.orthoHeight=number(j.at("orthographicHeight"));
    result.nearPlane=number(j.at("nearPlane"));result.farPlane=number(j.at("farPlane"));const auto offset=vector<2>(j.at("projectionOffset"));result.projectionOffsetX=offset[0];result.projectionOffsetY=offset[1];return result;
}
}
void to_json(nlohmann::json& j,const FogViewProjection& p){p.validate();j={{"schemaVersion",1},{"extent",p.extent},{"pixelOrigin",p.pixelOrigin==FogViewProjection::PixelOrigin::UpperLeft?"upper-left":"lower-left"},
    {"framePhase",p.framePhase},{"pixelCenterOffset",p.pixelCenterOffset},{"jitterRadius",p.jitterRadius},{"depthEncoding",p.depthEncoding},{"minimumViewDepth",p.minimumViewDepth},{"nearFadeWidth",p.nearFadeWidth}};}
void from_json(const nlohmann::json& j,FogViewProjection& p){
    fields(j,{"schemaVersion","extent","pixelOrigin","framePhase","pixelCenterOffset","jitterRadius","depthEncoding","minimumViewDepth","nearFadeWidth"});version(j);FogViewProjection result;
    const auto& extent=j.at("extent");if(!extent.is_array()||extent.size()!=2)throw std::invalid_argument("Invalid fog viewport");for(unsigned i=0;i<2;++i)result.extent[i]=integer(extent[i],16384);
    if(j.at("pixelOrigin")=="lower-left")result.pixelOrigin=FogViewProjection::PixelOrigin::LowerLeft;else if(j.at("pixelOrigin")!="upper-left")throw std::invalid_argument("Invalid fog pixel origin");
    result.framePhase=integer(j.at("framePhase"),7);result.pixelCenterOffset=number(j.at("pixelCenterOffset"));result.jitterRadius=number(j.at("jitterRadius"));result.depthEncoding=vector<3>(j.at("depthEncoding"));result.minimumViewDepth=number(j.at("minimumViewDepth"));result.nearFadeWidth=number(j.at("nearFadeWidth"));result.validate();p=result;
}
void to_json(nlohmann::json& j,const ViewFogBinding& p){p.validate();j={{"schemaVersion",1},{"asset",p.asset},{"enabled",p.enabled},{"projection",p.projection},{"captureView",p.captureView?encodeView(*p.captureView):Json(nullptr)}};}
void from_json(const nlohmann::json& j,ViewFogBinding& p){
    fields(j,{"schemaVersion","asset","enabled","projection","captureView"});version(j);ViewFogBinding result;
    if(!j.at("enabled").is_boolean()||!j.at("asset").is_string())throw std::invalid_argument("Invalid view fog resource or activation");
    result.asset=j.at("asset").get<std::string>();result.enabled=j.at("enabled").get<bool>();result.projection=j.at("projection").get<FogViewProjection>();
    if(!j.at("captureView").is_null())result.captureView=decodeView(j.at("captureView"));result.validate();p=std::move(result);
}
}
