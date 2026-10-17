#include "SceneExchange.hpp"
#include "scene/TransformEditing.hpp"
#include "assets/AssetTransfer.hpp"
#include <cmath>
#include <fstream>
#include <set>
namespace azurerender {
namespace {
void check(const std::function<bool()>& cancelled){if(cancelled&&cancelled())throw std::runtime_error("Scene import cancelled");}
std::string identity(const nlohmann::json& value,const char* field){auto id=value.at(field).get<std::string>();if(id.empty()||id.size()>128)throw std::invalid_argument("Invalid scene exchange identity");return id;}
std::array<float,3> vector(const nlohmann::json& value,const char* name,std::array<float,3> fallback){
 if(!value.contains(name))return fallback;const auto& source=value.at(name);if(!source.is_array()||source.size()!=3)throw std::invalid_argument("Transform vector requires three values");
 for(unsigned i=0;i<3;++i){fallback[i]=source.at(i).get<float>();if(!std::isfinite(fallback[i]))throw std::invalid_argument("Transform must be finite");}return fallback;
}
}
SceneImportProposal readSceneExchange(const std::filesystem::path& path,const std::function<bool()>& cancelled){
 check(cancelled);if(std::filesystem::file_size(path)>16*1024*1024)throw std::invalid_argument("Scene exchange exceeds 16 MiB budget");
 const auto sourceFingerprint=assetTransferFingerprint(path,cancelled);
 std::ifstream input(path);nlohmann::json data;input>>data;
 if(data.at("schemaVersion")!=1)throw std::invalid_argument("Unsupported scene exchange schemaVersion");
 if(data.value("upAxis",std::string())!="Y")throw std::invalid_argument("Scene exchange requires Y up");
 const auto units=data.value("unitMeters",1.F);if(!std::isfinite(units)||units<=0||units>100000)throw std::invalid_argument("Invalid scene exchange units");
 SceneImportProposal result;result.source=std::filesystem::weakly_canonical(path);result.sourceId=identity(data,"id");result.scene.sceneId=result.sourceId;
 result.sourceFingerprints[result.source]=sourceFingerprint;
 if(!data.at("resources").is_array()||!data.at("nodes").is_array()||data.at("nodes").size()>100000||data.at("resources").size()>10000)throw std::invalid_argument("Scene exchange table budget exceeded");
 std::set<std::string> resources,nodes,lights;
 for(const auto& resource:data.at("resources")){
  check(cancelled);auto id=identity(resource,"id");if(!resources.insert(id).second)throw std::invalid_argument("Duplicate scene resource ID");
  const auto relative=std::filesystem::u8path(resource.at("path").get<std::string>());if(relative.empty()||relative.is_absolute())throw std::invalid_argument("Resource path must be package-relative");
  const auto resolved=std::filesystem::weakly_canonical(result.source.parent_path()/relative);const auto safe=resolved.lexically_relative(result.source.parent_path());
  if(safe.empty()||*safe.begin()==".."||!std::filesystem::is_regular_file(resolved))throw std::invalid_argument("Missing or escaping scene resource: "+relative.u8string());
  if(resolved.extension()!=".glb"&&resolved.extension()!=".gltf")throw std::invalid_argument("Scene mesh requires glTF resource");
  if(std::filesystem::file_size(resolved)>512ULL*1024*1024)throw std::invalid_argument("Scene mesh exceeds 512 MiB budget");
  result.sourceFingerprints[resolved]=assetTransferFingerprint(resolved,cancelled);
  result.scene.resources.push_back({id,"gltf",resolved});
 }
 for(const auto& entry:data.at("nodes")){
  check(cancelled);SceneNode node;node.id=identity(entry,"id");if(!nodes.insert(node.id).second)throw std::invalid_argument("Duplicate scene node ID");
  node.name=entry.value("name",node.id);node.parentId=entry.value("parent",std::string());node.resourceId=entry.value("meshRef",std::string());
  if(!node.resourceId.empty()&&!resources.count(node.resourceId))throw std::invalid_argument("Missing mesh reference: "+node.resourceId);
  node.visible=entry.value("visible",true)&&!node.resourceId.empty();node.materialSlots=entry.value("materialOverrides",std::map<std::string,std::string>{});
  const auto transform=entry.value("localTransform",nlohmann::json::object());if(!transform.is_object())throw std::invalid_argument("Local transform requires an object");node.translation=vector(transform,"translation",{0,0,0});node.rotation=vector(transform,"rotationDegrees",{0,0,0});node.scale=vector(transform,"scale",{1,1,1});
  if(transform.contains("matrix")){
   const auto& values=transform.at("matrix");if(!values.is_array()||values.size()!=16)throw std::invalid_argument("Matrix requires 16 column-major values");
   internal::Matrix4 matrix{};for(unsigned i=0;i<16;++i)matrix[i]=values.at(i).get<float>();
   const auto trs=scene::decomposeTrs(matrix);node.translation=trs.translation;node.rotation=trs.rotation;node.scale=trs.scale;
  }
  for(auto& value:node.translation){value*=units;if(!std::isfinite(value))throw std::invalid_argument("Converted translation exceeds float range");}
  for(const auto value:node.scale)if(std::abs(value)<1e-7F)throw std::invalid_argument("Scene scale is singular");
  if(entry.contains("camera")){auto camera=entry.at("camera");camera["nearPlane"]=camera.value("nearPlane",.1F)*units;camera["farPlane"]=camera.value("farPlane",1000.F)*units;camera["orthographicHeight"]=camera.value("orthographicHeight",10.F)*units;result.components[node.id]["azure.camera"]={{"version",1},{"data",camera}};}
  result.scene.nodes.push_back(std::move(node));
 }
 std::map<std::string,std::string> parents;for(const auto& node:result.scene.nodes)parents[node.id]=node.parentId;
 std::set<std::string> completed;
 for(const auto& node:result.scene.nodes){std::set<std::string> chain;auto id=node.id;while(!id.empty()&&!completed.count(id)){check(cancelled);if(!parents.count(id))throw std::invalid_argument("Missing scene parent: "+id);if(!chain.insert(id).second)throw std::invalid_argument("Cyclic scene parent chain");id=parents.at(id);}completed.insert(chain.begin(),chain.end());}
 for(const auto& entry:data.value("lights",nlohmann::json::array())){
  check(cancelled);SceneLight light;light.id=identity(entry,"id");light.nodeId=entry.at("node").get<std::string>();if(!lights.insert(light.id).second||!nodes.count(light.nodeId))throw std::invalid_argument("Invalid scene light identity");
  const auto kind=entry.value("type",std::string("point"));
  if(kind!="point"&&kind!="spot"){result.diagnostics.push_back({{"code","UnsupportedLight"},{"source",light.id},{"message","Unsupported local light shape"}});continue;}
  light.shape=entry.value("shape",LightShape{});light.shape.type=kind=="spot"?LightKind::Spot:LightKind::Point;light.shape.validate();
  light.color=vector(entry,"color",{1,1,1});light.intensity=entry.value("intensity",1.F);light.radius=entry.value("radius",5.F)*units;light.enabled=entry.value("enabled",true);
  if(!std::isfinite(light.intensity)||light.intensity<0||!std::isfinite(light.radius)||light.radius<=0)throw std::invalid_argument("Invalid scene light range");result.scene.lights.push_back(light);
 }
 for(const auto& loss:data.value("losses",nlohmann::json::array()))result.diagnostics.push_back(loss);
 check(cancelled);if(assetTransferFingerprint(path,cancelled)!=sourceFingerprint)throw std::invalid_argument("Scene exchange changed during parse");return result;
}
}
