#include "editor/content/ContentOperationService.hpp"
#include "runtime/Project.hpp"
#include "assets/GenerationManifest.hpp"
#include "editor/EditorContext.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#ifdef _WIN32
#include <process.h>
#endif
using namespace azurerender;
int main(int argc,char** argv){
#ifdef _WIN32
 if(argc==3&&(std::string(argv[1])=="--interrupt"||std::string(argv[1])=="--interrupt-startup")) {
  const bool startup=std::string(argv[1])=="--interrupt-startup";
  ContentOperationService interrupted(Project::load(std::filesystem::u8path(argv[2])/"project.azureproject"));
  const auto pending=interrupted.plan({{"operation","asset.rename"},{"asset",startup?"assets:/startup.azscene":"assets:/interrupt.txt"},{"name",startup?"partial.azscene":"partial.txt"}});
  unsigned published=0;interrupted.apply(pending.at("id"),[&]{if(++published==(startup?4U:2U))std::_Exit(7);});return 2;
 }
#else
 static_cast<void>(argc);static_cast<void>(argv);
#endif
 const auto root=std::filesystem::temp_directory_path()/("azure-content-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
 const auto require=[](bool value,const char* message){if(!value)throw std::runtime_error(message);};
 const auto write=[](const auto& path,const std::string& text){std::ofstream(path,std::ios::binary)<<text;};
 try {
  Project::create(root,"Content transactions");write(root/"assets/data.txt","original");
  write(root/"assets/owner.azscript",R"({"schemaVersion":1,"type":"Example.Owner","assembly":"assets:/data.txt"})");
  SceneDocument::fromAsset("assets:/data.txt").save(root/"assets/linked.azscene");
  ContentOperationService service(Project::load(root/"project.azureproject"));
  const auto original=service.database().idForPath("assets:/data.txt");
  auto plan=service.plan({{"operation","asset.move"},{"asset",original},{"destination","assets:/moved.txt"}});
  require(std::filesystem::exists(root/"assets/data.txt")&&!std::filesystem::exists(root/"assets/moved.txt"),"Planning must preserve files");
  require(plan.at("affected").size()>=2,"Move must report reference owners");
  service.apply(plan.at("id"));
  require(service.database().idForPath("assets:/moved.txt")==original,"Move must preserve UUID");
  require(nlohmann::json::parse(service.database().readSource(service.database().idForPath("assets:/owner.azscript"))).at("assembly")=="assets:/moved.txt","Move must rewrite virtual references");
  require(SceneDocument::load(root/"assets/linked.azscene").resources[0].path.generic_string()=="assets:/moved.txt","Move must preserve references in text scene documents");
  service.undo();require(service.database().idForPath("assets:/data.txt")==original,"File undo must restore source and sidecar");
  service.redo();require(service.database().idForPath("assets:/moved.txt")==original,"File redo must republish stable identities and references");
  service.undo();
  auto copy=service.plan({{"operation","asset.duplicate"},{"asset",original},{"destination","assets:/copy.txt"}});service.apply(copy.at("id"));
  require(service.database().idForPath("assets:/copy.txt")!=original,"Duplicate must create a new UUID");
  auto stale=service.plan({{"operation","asset.rename"},{"asset",original},{"name","renamed.txt"}});write(root/"assets/data.txt","user write");
  bool rejected=false;try{service.apply(stale.at("id"));}catch(const std::exception&){rejected=true;}
  require(rejected&&std::filesystem::exists(root/"assets/data.txt")&&!std::filesystem::exists(root/"assets/renamed.txt"),"Expired plans must preserve user writes");
  rejected=false;try{service.plan({{"operation","asset.delete"},{"assets",{original}},{"referencePolicy","reject"}});}catch(const std::exception&){rejected=true;}
  require(rejected,"Referenced deletion must be rejected");
  rejected=false;try{service.plan({{"operation","asset.move"},{"asset",original},{"destination","engine:/assets_public/user.txt"}});}catch(const std::exception&){rejected=true;}
  require(rejected,"Engine mount must remain read-only");
  auto failed=service.plan({{"operation","asset.move"},{"asset",original},{"destination","assets:/uncommitted.txt"}});
  unsigned checkpoints=0;rejected=false;try{service.apply(failed.at("id"),[&]{if(++checkpoints==2)throw std::runtime_error("injected disk interruption");});}catch(const std::exception&){rejected=true;}
  require(rejected&&std::filesystem::exists(root/"assets/data.txt")&&!std::filesystem::exists(root/"assets/uncommitted.txt"),"Interrupted publication must recover all source files");
  ContentOperationService reopened(Project::load(root/"project.azureproject"));
  require(reopened.database().idForPath("assets:/data.txt")==original,"Recovery must retain stable identities");
  auto dir=reopened.plan({{"operation","content.create-directory"},{"parent","assets:/"},{"name","nested"}});reopened.apply(dir.at("id"));require(std::filesystem::is_directory(root/"assets/nested"),"Directory creation must publish safely");
  const auto owner=reopened.database().idForPath("assets:/owner.azscript");
  auto tree=reopened.plan({{"operation","asset.duplicate"},{"asset",owner},{"destination","assets:/nested/owner.azscript"},{"includeDependencies",true}});
  reopened.apply(tree.at("id"));
  const auto copiedOwner=reopened.database().idForPath("assets:/nested/owner.azscript");
  require(nlohmann::json::parse(reopened.database().readSource(copiedOwner)).at("assembly")=="assets:/nested/data.txt","Dependency copies must target their copied dependencies");
  require(reopened.database().idForPath("assets:/nested/data.txt")!=original,"Copied dependency identities must be distinct");
  reopened.undo();require(!std::filesystem::exists(root/"assets/nested/data.txt"),"Dependency tree undo must restore all files");
  write(root/"assets/a.png","texture A");write(root/"assets/b.png","texture B");
  ContentOperationService textures(Project::load(root/"project.azureproject"));
  auto batch=textures.plan({{"operation","texture.settings"},{"assets",{"assets:/a.png","assets:/b.png"}},{"values",{{"filter","nearest"}}}});textures.apply(batch.at("id"));
  require(nlohmann::json::parse(std::ifstream(root/"assets/a.png.azmeta")).at("settings").at("texture").at("filter")=="nearest","Texture batch must publish the selected fields");
  require(nlohmann::json::parse(std::ifstream(root/"assets/b.png.azmeta")).at("settings").at("texture").at("filter")=="nearest","Texture batch must include every selected image");
  textures.undo();
  const std::string binary("\x89PNG\xff\0",6);write(root/"assets/a.png",binary);
  auto binaryMove=textures.plan({{"operation","asset.rename"},{"asset","assets:/a.png"},{"name","binary.png"}});textures.apply(binaryMove.at("id"));
  textures.undo();
  {std::ifstream stream(root/"assets/a.png",std::ios::binary);require(std::string(std::istreambuf_iterator<char>(stream),{})==binary,"Binary file transactions must preserve arbitrary bytes through undo");}
  bool invalidBatch=false;try{textures.plan({{"operation","texture.settings"},{"assets",{"assets:/a.png","assets:/data.txt"}},{"values",{{"filter","nearest"}}}});}catch(const std::exception&){invalidBatch=true;}
  require(invalidBatch,"Texture settings must reject nonimage targets before publication");
  auto metadata=nlohmann::json::parse(std::ifstream(root/"assets/data.txt.azmeta"));
  GenerationManifest imported;imported.generatorId="azure.gltf-import";imported.licenseSource="LicenseRef-Test";imported.parametersHash=generationHash("test");
  imported.inputHashes["data.txt"]=generationHash("user write");imported.outputHashes=imported.inputHashes;
  metadata["importGeneration"]=imported.toJson();write(root/"assets/data.txt.azmeta",metadata.dump(2));
  ContentOperationService provenance(Project::load(root/"project.azureproject"));
  auto relocate=provenance.plan({{"operation","asset.move"},{"asset",original},{"destination","assets:/nested/imported.txt"}});provenance.apply(relocate.at("id"));
  require(provenance.database().records().at(original).importGeneration.has_value(),"Relocated imports must retain verifiable provenance");
  provenance.undo();
  auto durable=provenance.plan({{"operation","asset.rename"},{"asset",original},{"name","durable.txt"}});provenance.apply(durable.at("id"));
  ContentOperationService history(Project::load(root/"project.azureproject"));history.undo();
  require(std::filesystem::exists(root/"assets/data.txt")&&!std::filesystem::exists(root/"assets/durable.txt"),"File undo must retain committed journals across project reopen");
  ContentOperationService restoredRedo(Project::load(root/"project.azureproject"));restoredRedo.redo();
  require(std::filesystem::exists(root/"assets/durable.txt"),"File redo must retain undone journals across project reopen");restoredRedo.undo();history.recover();
  const auto startup=history.database().idForPath("assets:/startup.azscene");
  auto startupMove=history.plan({{"operation","asset.rename"},{"asset",startup},{"name","renamed.azscene"}});history.apply(startupMove.at("id"));
  require(Project::load(root/"project.azureproject").startupScene=="assets:/renamed.azscene","Startup moves must update the project descriptor");
  history.undo();
  bool collision=false;try{history.plan({{"operation","asset.rename"},{"asset",original},{"name","copy.txt"}});}catch(const std::exception&){collision=true;}
  require(collision,"Content plans must reject destination collisions");
  bool escape=false;try{history.plan({{"operation","asset.move"},{"asset",original},{"destination","assets:/../escape.txt"}});}catch(const std::exception&){escape=true;}
  require(escape,"Content plans must reject mount escapes");
  const auto writePermissions=std::filesystem::perms::owner_write|std::filesystem::perms::group_write|std::filesystem::perms::others_write;
  std::filesystem::permissions(root/"assets/data.txt",writePermissions,std::filesystem::perm_options::remove);
  bool readonly=false;try{history.plan({{"operation","asset.rename"},{"asset",original},{"name","readonly.txt"}});}catch(const std::exception&){readonly=true;}
  std::filesystem::permissions(root/"assets/data.txt",writePermissions,std::filesystem::perm_options::add);
  require(readonly,"Read-only source files must be rejected before publishing");
#ifdef _WIN32
  const auto crash=root/"interrupted-project";Project::create(crash,"Interrupted project");write(crash/"assets/interrupt.txt","crash source");
  ContentOperationService beforeCrash(Project::load(crash/"project.azureproject"));
  const auto crashIdentity=beforeCrash.database().idForPath("assets:/interrupt.txt");
  const auto executable=std::filesystem::absolute(argv[0]).u8string(),crashPath=crash.u8string();
  require(_spawnl(_P_WAIT,executable.c_str(),executable.c_str(),"--interrupt",crashPath.c_str(),nullptr)==7,"Recovery test must terminate during file publication");
  require(std::filesystem::exists(crash/"assets/partial.txt"),"Crash must occur after publishing a file");
  ContentOperationService recovered(Project::load(crash/"project.azureproject"));
  require(!std::filesystem::exists(crash/"assets/partial.txt")&&recovered.database().idForPath("assets:/interrupt.txt")==crashIdentity,"Restart must recover incomplete journals and identities");
  require(_spawnl(_P_WAIT,executable.c_str(),executable.c_str(),"--interrupt-startup",crashPath.c_str(),nullptr)==7,"Startup recovery must terminate after deleting the old startup file");
  require(!std::filesystem::exists(crash/"assets/startup.azscene"),"Startup interruption must temporarily invalidate normal project loading");
  const auto recoveredEditor=EditorContext::openProject(crash/"project.azureproject");
  require(recoveredEditor->scene().sceneId=="startup"&&std::filesystem::exists(crash/"assets/startup.azscene"),"Editor open must recover journals before validating the startup document");
#endif
  auto protectedUndo=history.plan({{"operation","asset.rename"},{"asset",original},{"name","protected.txt"}});history.apply(protectedUndo.at("id"));
  write(root/"assets/protected.txt","external change");bool overwritten=false;try{history.undo();}catch(const std::exception&){overwritten=true;}
  require(overwritten&&std::ifstream(root/"assets/protected.txt").good(),"Undo must reject external content changes");
  std::filesystem::remove_all(root);return 0;
 }catch(const std::exception& error){std::cerr<<error.what()<<'\n';std::filesystem::remove_all(root);return 1;}
}
