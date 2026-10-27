#include "runtime/AssetDatabase.hpp"
#include "render/OctahedralEnvironment.hpp"
#include "runtime/LevelSession.hpp"
#include <fstream>
#include <iterator>
#include <future>
#include <vector>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <chrono>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
using namespace azurerender;
void checkAt(bool value,int line) { if (!value) throw std::runtime_error("Asset/level contract failed at line "+std::to_string(line)); }
#define check(value) checkAt((value),__LINE__)
void write(const std::filesystem::path& p,const std::string& data) { std::ofstream(p) << data; }
int main() {
 const auto token=std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
 auto root=std::filesystem::temp_directory_path()/("azure_asset_level_tests-"+token);
 for(unsigned attempt=0;!std::filesystem::create_directory(root);++attempt)
     root=std::filesystem::temp_directory_path()/("azure_asset_level_tests-"+token+"-"+std::to_string(attempt));
 try {
  Project::create(root,"Assets");auto project=Project::load(root/"project.azureproject");
  write(root/"assets/data.txt","first");AssetDatabase db(project);db.refresh();
  const auto id=db.idForPath("assets:/data.txt");check(!id.empty() && db.readSource(id)=="first");
  check(db.refresh().empty() && db.refreshStatistics().sourceBytesRead==0);
  const auto stamp=std::filesystem::last_write_time(root/"assets/data.txt");
  write(root/"assets/data.txt","other");std::filesystem::last_write_time(root/"assets/data.txt",stamp);
  check(!db.refresh(true).empty() && db.readSource(id)=="other");
  write(root/"assets/data.txt","first");db.refresh(true);
  const auto keptRecords=db.records().size();bool cancelledRefresh=false;
  try{db.refresh(true,[]{throw std::runtime_error("cancelled scan");});}catch(const std::exception&){cancelledRefresh=true;}
  check(cancelledRefresh && db.records().size()==keptRecords && db.readSource(id)=="first");
  std::filesystem::rename(root/"assets/data.txt",root/"assets/moved.txt");
  std::filesystem::rename(root/"assets/data.txt.azmeta",root/"assets/moved.txt.azmeta");
  db.refresh();check(db.idForPath("assets:/moved.txt")==id && db.resolve(id).filename()=="moved.txt");
  // A cache publisher owns only its unique staging file. Another
  // operation's staging path must survive both publication and failure.
  const auto cachePath=db.cacheFile(id);
  const auto cacheRoot=cachePath.parent_path().parent_path();
  const auto cacheStamp=std::filesystem::file_time_type::clock::now()-std::chrono::hours(24);
  std::filesystem::last_write_time(cacheRoot,cacheStamp);
  AssetDatabase warmDatabase(project);warmDatabase.refresh();
  check(warmDatabase.readSource(id)=="first");
  if(std::filesystem::last_write_time(cacheRoot)!=cacheStamp)
      throw std::runtime_error("Warm refresh must reuse complete cache without creating staging data");
  const auto readCache=[&]{std::ifstream input(cachePath,std::ios::binary);
      return std::string(std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>());};
  const auto completePayload=readCache();auto damagedPayload=completePayload;damagedPayload.back()^=1;
  std::ofstream(cachePath,std::ios::binary).write(damagedPayload.data(),damagedPayload.size());
  AssetDatabase repairedDatabase(project);repairedDatabase.refresh();
  check(repairedDatabase.readSource(id)=="first" && readCache()==completePayload);
  const auto parallelRefresh=[&]{
      std::vector<std::future<void>> jobs;
      for(unsigned i=0;i<3;++i)jobs.push_back(std::async(std::launch::async,[&]{
          AssetDatabase candidate(project);candidate.refresh();
          check(candidate.readSource(id)=="first");
      }));
      for(auto& job:jobs)job.get();
      check(readCache()==completePayload);
  };
  std::filesystem::last_write_time(cacheRoot,cacheStamp);parallelRefresh();
  check(std::filesystem::last_write_time(cacheRoot)==cacheStamp);
  std::filesystem::remove(cachePath);parallelRefresh();
#ifdef _WIN32
  // A publisher retains delete access during rename. A cache reader must
  // permit that access while reading the complete immutable file.
  const auto publishingHandle=CreateFileW(cachePath.c_str(),DELETE,
      FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
  check(publishingHandle!=INVALID_HANDLE_VALUE);
  std::string concurrentRead;
  try{concurrentRead=db.readSource(id);}catch(...){CloseHandle(publishingHandle);throw;}
  CloseHandle(publishingHandle);check(concurrentRead=="first");
#endif
  const auto foreignTemporary=std::filesystem::path(cachePath.string()+".tmp");
  std::filesystem::remove(cachePath);
  std::filesystem::create_directory(foreignTemporary);
  write(foreignTemporary/"owner.txt","other operation");
  check(db.readSource(id)=="first");
  std::string owner;std::ifstream ownerFile(foreignTemporary/"owner.txt");std::getline(ownerFile,owner);ownerFile.close();
  check(owner=="other operation" && std::filesystem::is_directory(foreignTemporary));
  std::filesystem::remove(cachePath);std::filesystem::create_directory(cachePath);
  bool publicationRejected=false;
  try{AssetDatabase blockedPublisher(project);blockedPublisher.refresh();}
  catch(const std::exception&){publicationRejected=true;}
  check(publicationRejected && std::filesystem::is_directory(cachePath));
  for(const auto& entry:std::filesystem::directory_iterator(cachePath.parent_path().parent_path()))
    if(entry.path().extension()==".tmp")
      throw std::runtime_error("Failed cache publication must clean owned staging files");
  std::filesystem::remove(cachePath);check(db.readSource(id)=="first");
  std::filesystem::remove_all(foreignTemporary);
  write(cachePath,"existing incomplete cache");
  write(root/"assets/moved.txt","changed after snapshot");
  bool stalePublicationRejected=false;
  try{db.readSource(id);}catch(const std::exception&){stalePublicationRejected=true;}
  std::string preserved;std::ifstream priorCache(cachePath);std::getline(priorCache,preserved);priorCache.close();
  check(stalePublicationRejected && preserved=="existing incomplete cache");
  write(root/"assets/moved.txt","first");check(db.readSource(id)=="first");
  write(db.cacheFile(id),"corrupt");check(db.readSource(id)=="first");
  write(root/"assets/moved.txt","second");const auto changed=db.refresh();check(!changed.empty() && db.readSource(id)=="second");
  write(root/"assets/example.azscript",R"({"schemaVersion":1,"type":"Example.Script","assembly":"assets:/moved.txt"})");
  db.refresh();const auto scriptId=db.idForPath("assets:/example.azscript");
  check(db.records().at(scriptId).dependencies==std::vector<std::string>{id});
  write(root/"assets/example.azscript",R"({"schemaVersion":99,"type":"Example.Script"})");
  bool badScript=false;try{db.refresh(true);}catch(const std::exception&){badScript=true;}check(badScript);
  write(root/"assets/example.azscript",R"({"schemaVersion":1,"type":"Example.Script","assembly":"assets:/moved.txt"})");db.refresh();
  auto registry=reflection::makeRuntimeRegistry();ecs::TransformComponent transform;transform.translation={1,2,3};
  const auto component=registry.encode("azure.transform",&transform);
  nlohmann::json prefab={{"schemaVersion",1},{"id","prefab"},{"resources",nlohmann::json::array()},
   {"nodes",nlohmann::json::array({{{"id","node"},{"components",{{"azure.transform",component}}}}})}};
  write(root/"assets/body.azureprefab",prefab.dump());db.refresh();auto prefabId=db.idForPath("assets:/body.azureprefab");
  auto level=nlohmann::json::parse(R"({"schemaVersion":1,"id":"level","sceneType":"sample","resources":[{"id":"mesh","asset":"engine:/assets_public/test_model.gltf"}],"nodes":[],"prefabs":[{"instance":"instance","asset":"placeholder","overrides":{"node":{"components":{"azure.transform":{"data":{"translation":[7,8,9]}}}}}}]})");
  level["prefabs"][0]["asset"]=prefabId;
  ReflectionProbe probe;probe.texture="assets:/moved.txt";probe.halfExtents={3,4,5};probe.blendDistance=.75F;
  level["reflectionProbe"]=probe;
  write(root/"assets/field.aziv","irradiance-source");db.refresh();
  const auto giId=db.idForPath("assets:/field.aziv");
  level["irradianceVolume"]={{"version",2},{"asset",giId},{"intensity",.5},{"enabled",false},
      {"sampling",{{"normalOffset",.25},{"jitterAmplitude",.2},{"phase",49.875}}}};
  level["lights"]=nlohmann::json::array({{{"id","spot"},{"nodeId","instance:node"},
      {"shape",{{"type","spot"},{"direction",{0,0,-1}},{"innerCone",.25},{"outerCone",.5},
          {"falloff","range-power"},{"falloffExponent",3.5}}}}});
  write(root/"assets/start.azurelevel",level.dump());db.refresh();auto parsed=Level::load(root/"assets/start.azurelevel",db);
  check(parsed.scene.nodes.size()==1 && parsed.scene.nodes[0].translation[0]==7);
  check(parsed.scene.reflectionProbe.texture==db.resolve(id).u8string());
  check(parsed.scene.reflectionProbe.halfExtents==probe.halfExtents);
  check(parsed.scene.irradianceVolume.asset==db.resolve(giId).u8string());
  check(parsed.scene.irradianceVolume.intensity==.5F && !parsed.scene.irradianceVolume.enabled);
  check(parsed.scene.irradianceVolume.sampling.normalOffset==.25F && parsed.scene.irradianceVolume.sampling.phase==49.875F);
  const auto& giDependencies=db.records().at(db.idForPath("assets:/start.azurelevel")).dependencies;
  check(std::find(giDependencies.begin(),giDependencies.end(),giId)!=giDependencies.end());
  check(std::find(db.records().at(db.idForPath("assets:/start.azurelevel")).dependencies.begin(),
      db.records().at(db.idForPath("assets:/start.azurelevel")).dependencies.end(),id)!=db.records().at(db.idForPath("assets:/start.azurelevel")).dependencies.end());
  transform.translation={15,16,17};
  parsed.setComponent("instance:node","azure.transform",registry.encode("azure.transform",&transform),db);
  parsed.save(root/"saved.azurelevel");auto reopened=Level::load(root/"saved.azurelevel",db);
  check(reopened.scene.nodes[0].translation[2]==17 && reopened.scene.nodes[0].prefabSource==prefabId);
  check(reopened.scene.reflectionProbe.blendDistance==.75F);
  auto orientedLevel=level;orientedLevel["irradianceVolume"]["version"]=3;
  orientedLevel["irradianceVolume"]["sampling"]["pixelOrigin"]="bottom-left";
  orientedLevel["irradianceVolume"]["sampling"]["jitterSpace"]="field";
  write(root/"assets/oriented.azurelevel",orientedLevel.dump());db.refresh();
  auto orientedScene=Level::load(root/"assets/oriented.azurelevel",db);
  orientedScene.save(root/"oriented-saved.azurelevel");
  const auto orientedReloaded=Level::load(root/"oriented-saved.azurelevel",db);
  check(orientedReloaded.scene.irradianceVolume.sampling.pixelOrigin==IrradianceQueryParameters::PixelOrigin::BottomLeft);
  check(orientedReloaded.scene.irradianceVolume.sampling.jitterSpace==IrradianceQueryParameters::JitterSpace::Field);
  auto mappedLevel=level;
  mappedLevel["reflectionProbe"]["version"]=3;
  mappedLevel["reflectionProbe"]["lodMapping"]={{"schemaVersion",1},{"kind","logarithmic"},
      {"scale",1.2},{"bias",5},{"minimumRoughness",.001}};
  write(root/"assets/mapped.azurelevel",mappedLevel.dump());db.refresh();
  auto mappedScene=Level::load(root/"assets/mapped.azurelevel",db);mappedScene.save(root/"mapped-saved.azurelevel");
  const auto mappedReopened=Level::load(root/"mapped-saved.azurelevel",db);
  check(mappedReopened.scene.reflectionProbe.lodMapping.has_value());
  check(std::abs(mappedReopened.scene.reflectionProbe.lod(.5F,8)-3.8F)<2e-6F);
  OctahedralEnvironment array;array.layers=2;array.texels={0,0};const auto arrayBytes=array.encode();
  {std::ofstream out(root/"assets/probe.azoa",std::ios::binary);out.write(reinterpret_cast<const char*>(arrayBytes.data()),arrayBytes.size());}
  check(OctahedralEnvironment::load(root/"assets/probe.azoa").layers==2);
  auto fieldLevel=level;ReflectionProbe fieldProbe;fieldProbe.texture="assets:/probe.azoa";fieldProbe.field=ReflectionProbeField{};
  ReflectionProbeVolume room;room.arrayLayer=1;room.fade=.25F;fieldProbe.field->probes.push_back(room);
  fieldLevel["reflectionProbe"]=fieldProbe;write(root/"assets/field.azurelevel",fieldLevel.dump());db.refresh();
  auto fieldScene=Level::load(root/"assets/field.azurelevel",db);fieldScene.save(root/"field-saved.azurelevel");
  const auto fieldReopened=Level::load(root/"field-saved.azurelevel",db);
  check(fieldReopened.scene.reflectionProbe.field&&fieldReopened.scene.reflectionProbe.field->probes.size()==1);
  check(fieldReopened.scene.reflectionProbe.field->probes[0].fade==.25F);
  check(reopened.scene.irradianceVolume.sampling.jitterAmplitude==.2F && reopened.scene.irradianceVolume.sampling.phase==49.875F);
  check(reopened.scene.lights[0].shape.type==LightKind::Spot
      && reopened.scene.lights[0].shape.innerCone==.25F);
  check(reopened.scene.lights[0].shape.falloff==LightFalloff::RangePower
      && reopened.scene.lights[0].shape.falloffExponent==3.5F);
  check(nlohmann::json::object().get<LightShape>().falloff==LightFalloff::Legacy);
  auto directional=nlohmann::json{{"type","directional"},{"direction",{0,0,-1}}}.get<LightShape>();
  check(nlohmann::json(directional).at("type")=="directional");
  check(directional.angularWeight({1,0,0})==1.F);
  auto boxed=directional;boxed.specularScale=.195F;
  boxed.influenceBox.enabled=true;boxed.influenceBox.worldToBox[0][3]=3;
  auto boxedDocument=level;boxedDocument["lights"][0]["shape"]=boxed;
  auto boxedLevel=Level::parse(boxedDocument,db);boxedLevel.save(root/"assets/boxed.azurelevel");
  const auto reopenedBox=Level::load(root/"assets/boxed.azurelevel",db).scene.lights[0].shape.influenceBox;
  check(Level::load(root/"assets/boxed.azurelevel",db).scene.lights[0].shape.specularScale==.195F);
  check(reopenedBox.enabled&&reopenedBox.worldToBox[0][3]==3);
  auto characterDocument=level;
  characterDocument["lights"][0]["shape"]={{"type","point"},{"characterResponse",{{"schemaVersion",1},{"diffuseFloor",.25},{"compression",.8}}}};
  auto characterLevel=Level::parse(characterDocument,db);characterLevel.save(root/"assets/character-light.azurelevel");
  const auto characterLight=Level::load(root/"assets/character-light.azurelevel",db).scene.lights[0].shape.characterResponse;
  check(characterLight.present&&characterLight.diffuseFloor==.25F&&characterLight.compression==.8F);
  characterDocument["lights"][0]["shape"]["falloff"]="smooth-inverse-square";
  characterDocument["lights"][0]["shape"]["characterResponse"]["schemaVersion"]=2;
  characterDocument["lights"][0]["shape"]["characterResponse"]["linearLength"]=1;
  auto lineLevel=Level::parse(characterDocument,db);lineLevel.save(root/"assets/line-light.azurelevel");
  const auto lineLight=Level::load(root/"assets/line-light.azurelevel",db).scene.lights[0].shape.characterResponse;
  check(lineLight.present&&lineLight.schemaVersion==2&&lineLight.linearLength==1);
  characterDocument["lights"][0]["shape"]["falloff"]="range-power";
  characterDocument["lights"][0]["shape"]["characterResponse"]={{"schemaVersion",3},{"composition","color-blend"},
      {"strength",.7F},{"facingWeight",.25F},{"projectedDistance",false},{"order",8}};
  auto blendLevel=Level::parse(characterDocument,db);blendLevel.save(root/"assets/blend-light.azurelevel");
  const auto blendLight=Level::load(root/"assets/blend-light.azurelevel",db).scene.lights[0].shape.characterResponse;
  check(blendLight.isColorBlend()&&!blendLight.projectedDistance&&blendLight.strength==.7F&&blendLight.order==8);
  characterDocument["lights"][0]["shape"]["characterResponse"]={{"schemaVersion",4},{"composition","screen-rim"},
      {"offsetScale",.4F},{"surfaceTint",.8F}};
  auto rimLevel=Level::parse(characterDocument,db);rimLevel.save(root/"assets/rim-light.azurelevel");
  const auto rimLight=Level::load(root/"assets/rim-light.azurelevel",db).scene.lights[0].shape.characterResponse;
  check(rimLight.isCharacterOnly()&&rimLight.rimOffsetScale==.4F&&rimLight.rimSurfaceTint==.8F);
  check(!nlohmann::json::object().get<LightShape>().influenceBox.enabled);
  check(nlohmann::json::object().get<LightShape>().specularScale==1.F);
  const auto responseShape=nlohmann::json{{"specularScale",.195F}}.get<LightShape>();
  check(responseShape.specularScale==.195F);
  bool rejectedResponse=false;try{(void)nlohmann::json{{"specularScale",-1}}.get<LightShape>();}catch(const std::exception&){rejectedResponse=true;}
  check(rejectedResponse);
  for(const auto& invalidBox:{nlohmann::json{{"falloffThreshold",-1}},
          nlohmann::json{{"enabled",true},{"worldToBox",{{0,0,0,0},{0,0,0,0},{0,0,0,0}}}}}) {
    bool rejected=false;try{(void)invalidBox.get<LightInfluenceBox>();}catch(const std::exception&){rejected=true;}check(rejected);
  }
  auto directionalDocument=level;
  directionalDocument["lights"][0]["shape"]=directional;
  auto directionalLevel=Level::parse(directionalDocument,db);
  directionalLevel.save(root/"assets/directional.azurelevel");
  check(Level::load(root/"assets/directional.azurelevel",db).scene.lights[0].shape.type==static_cast<LightKind>(2));
  for(const auto& bad : {nlohmann::json{{"falloff","unknown"}},
          nlohmann::json{{"falloff","range-power"},{"falloffExponent",0}}}) {
    bool rejected=false;
    try { (void)bad.get<LightShape>(); } catch(const std::exception&) {rejected=true;}
    check(rejected);
  }
  std::filesystem::create_directory(root/"blocked.azurelevel");
  bool saveFailed=false;try { parsed.save(root/"blocked.azurelevel"); }catch(const std::exception&){saveFailed=true;}
  check(saveFailed && std::filesystem::is_directory(root/"blocked.azurelevel"));
  for(const auto& entry:std::filesystem::directory_iterator(root))
    if(entry.path().filename().string().find(".tmp")!=std::string::npos)
      throw std::runtime_error("Failed level save must clean temporary files");
  write(root/"assets/body.azureprefab",prefab.dump(2));auto dependencyChanges=db.refresh();
  check(std::find(dependencyChanges.begin(),dependencyChanges.end(),db.idForPath("assets:/start.azurelevel"))!=dependencyChanges.end());
  db.writePack(root/"pack");check(AssetDatabase::resolvePack(root/"pack",id).filename()=="moved.txt");
  check(AssetDatabase::resolvePack(root/"pack",giId).filename()=="field.aziv");
  write(root/"pack/assets/moved.txt","damage");bool corrupt=false;try { AssetDatabase::resolvePack(root/"pack",id); } catch (const std::exception&) { corrupt=true; }check(corrupt);
  RuntimeLifecycle runtime;runtime.start();LevelSession session(project,runtime);session.request("assets:/start.azurelevel");
  auto settle=[&]{const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(5);bool committed=false;
    do{committed=session.poll()||committed;std::this_thread::yield();}while(session.loading()&&std::chrono::steady_clock::now()<end);check(!session.loading());return committed;};
  check(settle() && runtime.snapshotScene().nodes[0].translation[0]==7);
  // A file edited after the active database snapshot must not prevent a
  // freshly prepared destination from committing.
  write(root/"assets/moved.txt","edited-before-switch");
  session.request("assets:/startup.azscene");
  check(settle() && session.currentReference()=="assets:/startup.azscene");
  check(session.assets().readSource(id)=="edited-before-switch");
  write(root/"assets/moved.txt","second");session.assets().refresh(true);
  session.request("assets:/start.azurelevel");check(settle());
  session.preload("assets:/startup.azscene");settle();
  check(session.preloaded("assets:/startup.azscene"));
  int uploadPolls=0;
  std::filesystem::copy_file(root/"assets/startup.azscene",root/"assets/preload.azscene");
  session.setPreloadHandler([&](const Level& candidate){
    for(const auto& resource:candidate.scene.resources)
      check(candidate.preparedMeshes.count(resource.path.lexically_normal().generic_string())!=0);
    return ++uploadPolls>=3;});
  session.preload("assets:/preload.azscene");settle();
  check(uploadPolls==3 && session.preloaded("assets:/preload.azscene"));
  session.setReadinessHandler([](const Level&){return false;});
  check(!session.preloaded("assets:/preload.azscene"));
  session.setReadinessHandler({});
  session.setPreloadHandler({});
  write(root/"assets/moved.txt","third");session.assets().refresh(true);
  check(!session.preloaded("assets:/startup.azscene"));
  write(root/"assets/moved.txt","second");session.assets().refresh(true);
  const auto originalRevision=session.revision();session.request("assets:/startup.azscene");session.cancelPending();settle();
  check(session.revision()==originalRevision && runtime.snapshotScene().sceneId=="level");
  session.request("assets:/missing.azurelevel");session.poll();session.cancelPending();settle();
  check(session.lastError().empty() && runtime.snapshotScene().sceneId=="level");
  session.request("assets:/startup.azscene");session.request("assets:/start.azurelevel");settle();
  check(runtime.snapshotScene().sceneId=="level");
  session.setPrepareHandler([](const Level&) { throw std::runtime_error("GPU preparation failed"); });
  session.request("assets:/startup.azscene");check(!settle() && runtime.snapshotScene().sceneId=="level");
  session.setPrepareHandler({});
  const auto before=runtime.world().entityCount();session.request("assets:/missing.azurelevel");check(!settle());
  check(!session.lastError().empty() && runtime.world().entityCount()==before);
  auto broken=level;broken["prefabs"][0]["overrides"]["unknown"]={{"name","bad"}};write(root/"bad.azurelevel",broken.dump());
  bool bad=false;try { Level::load(root/"bad.azurelevel",db); }catch(const std::exception&){bad=true;}check(bad);
  write(root/"assets/cycle.azureprefab",prefab.dump());
  write(root/"assets/cycle.azureprefab.azmeta",R"({"schemaVersion":1,"id":"ffffffff-ffff-4fff-8fff-ffffffffffff"})");
  db.refresh();auto cycleId=db.idForPath("assets:/cycle.azureprefab");
  auto cycle=prefab;cycle["prefabs"]=nlohmann::json::array({{{"asset",cycleId},{"instance","cycle"}}});
  write(root/"assets/cycle.azureprefab",cycle.dump());
  write(root/"assets/moved.txt","uncommitted");
  bool cycleRejected=false;try { db.refresh(); }catch(const std::exception&){cycleRejected=true;}
  check(cycleRejected && db.readSource(id)=="second");
  std::cout << "Asset relocation, cache repair, dependencies, prefab overrides, pack and transactional level switch passed\n";
 } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';std::filesystem::remove_all(root);return 1;}
 std::filesystem::remove_all(root);
}
