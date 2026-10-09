#include "editor/EditorSession.hpp"
#include "runtime/Prefab.hpp"
#include "editor/documents/DocumentBatchTransaction.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
using namespace azurerender;using Json=nlohmann::json;
void require(bool v,const char* m){if(!v)throw std::runtime_error(m);}
Json edit(EditorSession& s,const std::string& id,Json a=Json::object()){const auto r=s.edit(id,a);if(!r)throw std::runtime_error(id+": "+r.diagnostics.dump());return r.value;}
Json read(const std::filesystem::path& p){std::ifstream f(p);Json v;f>>v;return v;}
int main(int argc,char** argv){if(argc!=2)return 2;const auto root=std::filesystem::temp_directory_path()/("azure-prefab-authoring-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
try{
    std::filesystem::copy(argv[1],root,std::filesystem::copy_options::recursive);
    auto context=EditorContext::openProject(root/"project.azureproject");EditorSession s(context);const auto original=context->documentContent();
    const auto created=edit(s,"prefab.create",{{"nodes",{"fixed","moving","joint"}},{"path","assets:/arm.azureprefab"}});
    const auto asset=created.at("asset").get<std::string>();require(context->documentContent()==original&&!context->dirty(),"Creating reusable source preserves current scene");
    require(!s.edit("prefab.create",{{"nodes",{"fixed"}},{"path","assets:/arm.azureprefab"}}),"Occupied source is protected");
    require(!s.edit("prefab.create",{{"nodes",{"joint"}},{"path","assets:/bad.azureprefab"}})&&!std::filesystem::exists(root/"assets/bad.azureprefab"),"Prefab creation rejects external node references");
    edit(s,"prefab.place",{{"asset","assets:/arm.azureprefab"},{"instance","one"}});
    require(context->componentData("one:joint","azure.hinge").at("bodyA")=="one:fixed"&&context->componentData("one:joint","azure.hinge").at("bodyB")=="one:moving","Registered node references map to instance scope");
    edit(s,"document.save");const auto initial=read(context->scenePath());require(initial.at("prefabs")[0].value("overrides",Json::object()).empty(),"Saving untouched instance keeps source fields live");
    edit(s,"node.select",{{"id","one:moving"}});edit(s,"node.rename",{{"value","Authored arm"}});
    auto overrides=edit(s,"prefab.overrides",{{"instance","one"}});
    require(overrides.at("fields").size()==1&&overrides.at("fields")[0].at("node")=="moving"&&overrides.at("fields")[0].at("path")=="/name","Override list contains only changed field");
    const auto fields=Json::array({{{"node","moving"},{"path","/name"}}});
    edit(s,"prefab.revert",{{"instance","one"},{"fields",fields}});require(context->selectedNode()->name=="moving","Revert restores selected source field");
    edit(s,"history.undo");require(context->selectedNode()->name=="Authored arm","Revert shares document undo");edit(s,"document.save");
    std::filesystem::copy_file(context->scenePath(),root/"assets/other.azurelevel");context->assets().refresh();
    const auto first=context->documentId();edit(s,"document.open",{{"path","assets:/other.azurelevel"}});s.pollTasks();const auto second=s.context().documentId();
    edit(s,"node.select",{{"id","one:moving"}});edit(s,"component.field",{{"type","azure.rigid-body"},{"field","radius"},{"value",2.}});
    edit(s,"prefab.revert",{{"instance","one"},{"fields",fields}});require(s.context().selectedNode()->name=="moving","Second document keeps an independent name value");
    edit(s,"document.activate",{{"id",first}});s.pollTasks();overrides=edit(s,"prefab.overrides",{{"instance","one"}});const auto fingerprint=overrides.at("baseFingerprint");
    require(!s.edit("prefab.apply",{{"instance","one"},{"fields",fields},{"baseFingerprint","expired"}}),"Apply rejects stale source fingerprint");
    const auto result=edit(s,"prefab.apply",{{"instance","one"},{"fields",fields},{"baseFingerprint",fingerprint}});require(result.at("documents").size()==2,"Source application checks all retained documents");
    require(read(root/"assets/arm.azureprefab").at("nodes")[1].at("name")=="Authored arm","Apply persists selected field in source");
    require(edit(s,"prefab.overrides",{{"instance","one"}}).at("fields").empty(),"Applied field becomes source value");
    edit(s,"document.activate",{{"id",second}});s.pollTasks();require(s.context().componentData("one:moving","azure.rigid-body").at("radius")==2.,"Source application preserves unrelated document overrides");
    require(s.context().selectedNode()->name=="Authored arm","Unmodified source fields propagate to retained documents");
    require(edit(s,"prefab.overrides",{{"instance","one"}}).at("fields").size()==1,"Only independent override remains in another document");
    edit(s,"prefab.undo-apply");require(read(root/"assets/arm.azureprefab").at("nodes")[1].at("name")=="moving","Source transaction has explicit undo");
    require(s.context().selectedNode()->name=="moving","Source undo updates the invoking document through shared editing semantics");
    edit(s,"document.activate",{{"id",first}});s.pollTasks();require(edit(s,"prefab.overrides",{{"instance","one"}}).at("fields").size()==1,"Source undo restores original instance override");
    edit(s,"prefab.redo-apply");require(read(root/"assets/arm.azureprefab").at("nodes")[1].at("name")=="Authored arm","Source application has version checked redo");
    edit(s,"prefab.undo-apply");require(read(root/"assets/arm.azureprefab").at("nodes")[1].at("name")=="moving","Repeated source undo restores original bytes");
    auto& other=*s.documents().at(second).context;const auto firstBefore=context->documentVersion(),secondBefore=other.documentVersion();
    DocumentBatchTransaction batch({context.get(),&other});batch.candidate(*context).selectedNode()->name="Candidate only";
    bool rollback=false,rejected=false;try{batch.commit([]{throw std::runtime_error("Publication interrupted");},[&]{rollback=true;});}catch(const std::runtime_error&){rejected=true;}
    require(rejected&&rollback&&context->documentVersion()==firstBefore&&other.documentVersion()==secondBefore,"Interrupted publication invokes recovery and preserves all documents");
    DocumentBatchTransaction recovery({context.get(),&other});recovery.candidate(*context).selectedNode()->name="Rejected candidate";
    rejected=false;try{recovery.commit([&]{context->scene().nodes.front().name="Partial publish";throw std::runtime_error("Publish failed");},[]{throw std::runtime_error("Rollback failed");});}catch(const std::runtime_error&){rejected=true;}
    require(rejected&&context->documentVersion()==firstBefore&&other.documentVersion()==secondBefore,"Rollback callback failure still restores all document contents and histories");
    DocumentBatchTransaction stale({context.get(),&other});edit(s,"node.rename",{{"value","Concurrent edit"}});const auto concurrent=context->documentVersion();
    bool published=false;rejected=false;try{stale.commit([&]{published=true;},[]{});}catch(const std::runtime_error&){rejected=true;}
    require(rejected&&!published&&context->documentVersion()==concurrent,"Stale candidate rejects before publication and preserves concurrent edits");edit(s,"history.undo");
    const auto afterConcurrent=context->documentVersion();
    const auto originalParent=other.scene().nodes.front().parentId;other.scene().nodes.front().parentId="unknown-parent";
    const auto invalidVersion=other.documentVersion();const auto sourceBefore=read(root/"assets/arm.azureprefab");
    const auto validFingerprint=edit(s,"prefab.overrides",{{"instance","one"}}).at("baseFingerprint");
    require(!s.edit("prefab.apply",{{"instance","one"},{"fields",fields},{"baseFingerprint",validFingerprint}}),"Invalid retained document rejects shared source application");
    require(read(root/"assets/arm.azureprefab")==sourceBefore&&context->documentVersion()==afterConcurrent&&other.documentVersion()==invalidVersion,"Failed multi document validation preserves source, history and versions");other.scene().nodes.front().parentId=originalParent;
    auto changedSource=sourceBefore;changedSource["nodes"][0]["name"]="External source edit";{std::ofstream out(root/"assets/arm.azureprefab");out<<changedSource.dump(2);}
    require(edit(s,"prefab.overrides",{{"instance","one"}}).at("sourceChanged").get<bool>(),"External source edit is observed");
    const auto beforePlacement=context->documentVersion();require(!s.edit("prefab.place",{{"asset",asset},{"instance","two"}}),"Placement rejects a stale source before changing existing instances");
    require(context->documentVersion()==beforePlacement,"Rejected placement preserves existing content and history");
    require(edit(s,"prefab.overrides",{{"instance","one"}}).at("sourceChanged").get<bool>(),"Placing another instance retains existing source conflict baseline");
    context->reload();edit(s,"prefab.place",{{"asset",asset},{"instance","two"}});
    require(!edit(s,"prefab.overrides",{{"instance","two"}}).at("sourceChanged").get<bool>(),"New instance captures current source baseline");
    std::filesystem::remove_all(root);std::cout<<"Prefab source, references, override fields and multi document transactions passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';std::filesystem::remove_all(root);return 1;}}
