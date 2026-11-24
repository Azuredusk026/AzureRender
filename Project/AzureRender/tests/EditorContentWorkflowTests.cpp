#include "editor/EditorSession.hpp"
#include "runtime/ComponentRegistry.hpp"
#include "scene/TransformSystem.hpp"
#include <chrono>
#include <cmath>
#include <iostream>
using namespace azurerender;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void apply(EditorSession& session,const std::string& command,Json parameters=Json::object()) {
    const auto result=session.edit(command,std::move(parameters));
    if(!result)throw std::runtime_error(command+": "+result.diagnostics.dump());
}
std::shared_ptr<EditorContext> fixture(const std::filesystem::path& path) {
    SceneDocument scene;scene.sceneId="content";
    SceneNode parent;parent.id="parent";parent.name="Assembly";parent.translation={3,0,0};parent.rotation={0,90,0};parent.scale={2,3,4};
    SceneNode child;child.id="child";child.name="Part";child.parentId="parent";child.translation={1,2,0};
    SceneNode camera;camera.id="camera";camera.name="Camera";camera.parentId="parent";
    SceneNode external;external.id="external";external.name="External";
    scene.nodes={parent,child,camera,external};
    scene.lights.push_back({"part-light","child",{1,1,1},2,4,true});
    return std::make_shared<EditorContext>(scene,path);
}
void subtree(const std::filesystem::path& root) {
    auto context=fixture(root/"subtree.azscene");EditorSession session(context);
    session.selection().set({"camera"});apply(session,"component.add",{{"type","azure.third-person-camera"}});
    apply(session,"component.field",{{"type","azure.third-person-camera"},{"field","target"},{"value","child"}});
    session.selection().set({"external"});apply(session,"component.add",{{"type","azure.third-person-camera"}});
    apply(session,"component.field",{{"type","azure.third-person-camera"},{"field","target"},{"value","child"}});
    session.selection().set({"parent","child"});const auto before=context->documentContent();const auto count=context->undoCount();
    apply(session,"node.duplicate");
    require(context->scene().nodes.size()==7,"Copying parent and child must produce exactly one complete subtree");
    std::string parent,child,camera;
    for(std::size_t i=4;i<context->scene().nodes.size();++i){const auto& node=context->scene().nodes[i];
        if(node.name=="Assembly Copy")parent=node.id;if(node.name=="Part Copy")child=node.id;if(node.name=="Camera Copy")camera=node.id;}
    require(!parent.empty()&&!child.empty()&&!camera.empty(),"Every subtree node must have a unique copy");
    require(context->componentData(camera,"azure.third-person-camera").at("target")==child,"Internal node references must map to the copied child");
    require(context->componentData("external","azure.third-person-camera").at("target")=="child","External references must retain their original identity");
    require(context->scene().lights.size()==2&&context->scene().lights.back().nodeId==child&&context->scene().lights.back().id!="part-light","Copied lights must have a unique identity and copied owner");
    require(context->undoCount()==count+1,"Subtree copy must form one undo unit");
    apply(session,"history.undo");require(context->documentContent()==before,"Undo must restore the entire original hierarchy");
    session.selection().set({"external"});apply(session,"node.duplicate");
    require(context->componentData(context->selectedNode()->id,"azure.third-person-camera").at("target")=="child","Copied external references must retain the original target");
}
void reparent(const std::filesystem::path& root) {
    auto context=fixture(root/"reparent.azscene");EditorSession session(context);
    const auto before=scene::resolveNodeWorldTransforms(context->scene().renderDescription())[1];
    apply(session,"node.reparent",{{"id","child"},{"parent","external"}});
    const auto after=scene::resolveNodeWorldTransforms(context->scene().renderDescription())[1];
    for(unsigned i=0;i<16;++i)require(std::abs(before[i]-after[i])<.001F,"Reparent must preserve world position, rotation and scale");
    apply(session,"history.undo");const auto bytes=context->documentContent();const auto count=context->undoCount();
    require(!session.edit("node.reparent",{{"id","parent"},{"parent","child"}}),"A descendant cannot become its ancestor's parent");
    require(context->documentContent()==bytes&&context->undoCount()==count,"Rejected cycle must preserve content and history");
    session.selection().set({"external"});apply(session,"node.transform",{{"rotation",{0,45,0}},{"scale",{1,2,3}}});
    const auto shearBytes=context->documentContent();
    require(!session.edit("node.reparent",{{"id","child"},{"parent","external"}}),"Reparent must reject unrepresentable shear");
    require(context->documentContent()==shearBytes,"Shear rejection must preserve the hierarchy and transforms");
}
void components(const std::filesystem::path& root) {
    auto context=fixture(root/"components.azscene");EditorSession session(context);session.selection().set({"child"});
    apply(session,"component.add",{{"type","azure.character"}});
    apply(session,"component.field",{{"type","azure.character"},{"field","speed"},{"value",12}});
    apply(session,"component.reset-field",{{"type","azure.character"},{"field","speed"}});
    require(context->componentData("child","azure.character").at("speed")==4,"Field reset must consume registered component defaults");
    apply(session,"history.undo");require(context->componentData("child","azure.character").at("speed")==12,"Field reset must be undoable");
    apply(session,"component.remove",{{"type","azure.character"}});
    require(context->componentData("child","azure.character").is_null(),"Component removal must update the document");
    apply(session,"history.undo");require(context->componentData("child","azure.character").at("speed")==12,"Removal undo must restore all component values");
    require(!session.edit("component.remove",{{"type","azure.transform"}}),"Mandatory transform removal must have a clear rejection");
    apply(session,"component.add",{{"type","azure.third-person-camera"}});
    apply(session,"component.field",{{"type","azure.third-person-camera"},{"field","target"},{"value","parent"}});
    apply(session,"component.reset-field",{{"type","azure.third-person-camera"},{"field","target"}});
    require(context->componentData("child","azure.third-person-camera").at("target")=="child","Declared selected-node default must remain valid on arbitrary projects");
}
struct LinkedNote { std::string target,note="default note"; };
void registered(const std::filesystem::path& root) {
    using namespace reflection;
    runtimeComponentRegistry().registerComponent<LinkedNote>(reflectedType<LinkedNote>("tool.linked-note",1,{
        withReference(property<LinkedNote>("target","Target",&LinkedNote::target,0,0),"node",{},"selected-node"),
        property<LinkedNote>("note","Note",&LinkedNote::note,0,0)}));
    auto context=fixture(root/"registered.azscene");EditorSession session(context);session.selection().set({"child"});
    apply(session,"component.add",{{"type","tool.linked-note"}});
    require(context->componentData("child","tool.linked-note").at("target")=="child","Extension defaults must consume declared reference policy");
    const auto longText=std::string(2048,'x');apply(session,"component.field",{{"type","tool.linked-note"},{"field","note"},{"value",longText}});
    session.selection().set({"parent"});apply(session,"node.duplicate");
    for(const auto& node:context->scene().nodes)if(node.name=="Part Copy"){
        const auto fields=context->componentData(node.id,"tool.linked-note");
        require(fields.at("target")==node.id&&fields.at("note")==longText,"Dynamic reference mappings must preserve complete extension fields");
        const auto id=node.id;session.selection().set({id});apply(session,"component.reset-field",{{"type","tool.linked-note"},{"field","note"}});
        require(context->componentData(id,"tool.linked-note").at("note")=="default note","Extension field reset must use registered defaults");break;
    }
}
void queries(const std::filesystem::path& root) {
    auto context=fixture(root/"queries.azscene");EditorSession session(context);session.selection().set({"child"});const auto before=context->documentContent();const auto undo=context->undoCount();
    require(static_cast<bool>(session.edit("node.rename",{{"value","Part a"}},"name-edit")),"First name input must succeed");
    apply(session,"assets.catalog");apply(session,"tasks.describe");apply(session,"feedback.describe");
    require(static_cast<bool>(session.edit("node.rename",{{"value","Part ab"}},"name-edit")),"Second name input must succeed");
    require(context->undoCount()==undo+1,"Read-only panel queries must preserve one continuous edit undo unit");
    apply(session,"history.end-edit");
    require(static_cast<bool>(session.edit("node.rename",{{"value","Part abc"}},"name-edit")),"A new typing session must succeed");
    require(context->undoCount()==undo+2,"Explicit edit boundaries must separate independent typing sessions");
    apply(session,"history.undo");require(context->selectedNode()->name=="Part ab","Undo must first restore the preceding typing session");
    apply(session,"history.undo");require(context->documentContent()==before,"A single continuous edit undo must restore the name before typing");
}
void metadata(const std::filesystem::path&) {
    const auto camera=runtimeComponentRegistry().describe("azure.third-person-camera");
    const auto& target=camera.at("properties").at(0);
    require(target.value("reference",std::string())=="node","Camera target must publish node reference metadata");
    const auto script=runtimeComponentRegistry().describe("azure.script");
    require(script.at("properties").at(0).value("reference",std::string())=="asset","Script fields must publish asset reference metadata");
}
void feedback(const std::filesystem::path& root) {
    auto context=fixture(root/"feedback.azscene");EditorSession session(context);
    require(!session.edit("node.reparent",{{"id","missing"},{"parent","child"}}),"Unknown node must fail");
    const auto message=session.lastError();require(!message.empty(),"Failure must have a diagnostic");
    apply(session,"viewport.gizmo-mode",{{"value",1}});
    require(session.lastError()==message,"Successful read-only UI work must retain unrelated errors");
    apply(session,"feedback.dismiss");require(session.lastError().empty(),"Explicit dismissal must close the error");
}
}
int main() {
    const auto root=std::filesystem::temp_directory_path()/("azure-content-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root);int failures=0;
    for(const auto& [name,test]:std::vector<std::pair<const char*,void(*)(const std::filesystem::path&)>>{
        {"subtree",subtree},{"reparent",reparent},{"components",components},{"metadata",metadata},{"feedback",feedback},{"registered",registered},{"queries",queries}}){
        try{test(root);std::cout<<name<<": passed\n";}catch(const std::exception& error){std::cerr<<name<<": "<<error.what()<<'\n';++failures;}}
    std::filesystem::remove_all(root);return failures?1:0;
}
