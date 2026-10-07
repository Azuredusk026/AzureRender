#include "runtime/AssetDatabase.hpp"
#include "runtime/LevelSession.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
using namespace azurerender;
void check(bool value) { if (!value) throw std::runtime_error("Asset/level contract failed"); }
void write(const std::filesystem::path& p,const std::string& data) { std::ofstream(p) << data; }
int main() {
 auto root=std::filesystem::temp_directory_path()/"azure_asset_level_tests";
 std::filesystem::remove_all(root);
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
  write(root/"assets/start.azurelevel",level.dump());db.refresh();auto parsed=Level::load(root/"assets/start.azurelevel",db);
  check(parsed.scene.nodes.size()==1 && parsed.scene.nodes[0].translation[0]==7);
  transform.translation={15,16,17};
  parsed.setComponent("instance:node","azure.transform",registry.encode("azure.transform",&transform),db);
  parsed.save(root/"saved.azurelevel");auto reopened=Level::load(root/"saved.azurelevel",db);
  check(reopened.scene.nodes[0].translation[2]==17 && reopened.scene.nodes[0].prefabSource==prefabId);
  write(root/"assets/body.azureprefab",prefab.dump(2));auto dependencyChanges=db.refresh();
  check(std::find(dependencyChanges.begin(),dependencyChanges.end(),db.idForPath("assets:/start.azurelevel"))!=dependencyChanges.end());
  db.writePack(root/"pack");check(AssetDatabase::resolvePack(root/"pack",id).filename()=="moved.txt");
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
