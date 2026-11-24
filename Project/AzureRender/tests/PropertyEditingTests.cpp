#include "editor/EditorSession.hpp"
#include "editor/properties/PropertyEditorRegistry.hpp"
#include "runtime/ComponentRegistry.hpp"
#include <chrono>
#include <iostream>
using namespace azurerender;
struct EditorProbe {float weight=2;float locked=1;float single=3;};
int main(){try{
    auto require=[](bool ok,const char* message){if(!ok)throw std::runtime_error(message);};
    SceneDocument scene;scene.sceneId="properties";
    SceneNode a;a.id="a";a.name="First";a.translation={1,2,3};
    SceneNode b;b.id="b";b.name="Second";b.translation={4,5,6};scene.nodes={a,b};
    const auto path=std::filesystem::temp_directory_path()/("azure-property-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".azscene");
    auto context=std::make_shared<EditorContext>(scene,path);EditorSession session(context);session.selection().set({"a","b"});
    const auto fields=PropertyEditorRegistry::selection(*context,{"a","b"},"azure.transform");
    require(fields.at("translation").at("mixed").get<bool>(),"Different positions must report mixed values");
    require(fields.at("translation").at("unit")=="m","Position must declare metres");
    require(fields.at("rotation").at("unit")=="deg","Rotation must declare degrees");
    const auto base=context->documentContent();const auto undo=context->undoCount();
    require(static_cast<bool>(session.edit("component.batch-field",{{"nodes",{"a","b"}},{"type","azure.transform"},{"field","translation"},{"axis",0},{"value",0}})),"Axis reset must edit all selected objects");
    require(context->scene().nodes[0].translation==std::array<float,3>{0,2,3}&&context->scene().nodes[1].translation==std::array<float,3>{0,5,6},"Axis edits must preserve other mixed coordinates");
    require(context->undoCount()==undo+1&&session.selection().selected()==std::vector<std::string>{"a","b"},"Batch edit must keep selection and create one undo");
    session.edit("history.undo");require(context->documentContent()==base,"Undo must restore both targets");
    session.edit("history.redo");require(context->scene().nodes[1].translation[0]==0,"Redo must restore the batch");
    const auto after=context->documentContent();
    require(!session.edit("component.batch-field",{{"nodes",{"a","missing"}},{"type","azure.transform"},{"field","translation"},{"value",{9,9,9}}}),"An invalid target must reject the whole batch");
    require(context->documentContent()==after,"A partial target failure must preserve every target");
    require(!session.edit("component.batch-field",{{"nodes",{"a","b"}},{"type","azure.transform"},{"field","scale"},{"axis",1},{"value",0}}),"Zero scale must reject all targets");
    require(context->documentContent()==after,"Illegal values must preserve the batch");
    const auto beforeMerge=context->undoCount();
    session.edit("component.batch-field",{{"nodes",{"a","b"}},{"type","azure.transform"},{"field","translation"},{"axis",2},{"value",7}},"batch-z");
    session.edit("component.batch-field",{{"nodes",{"a","b"}},{"type","azure.transform"},{"field","translation"},{"axis",2},{"value",8}},"batch-z");
    require(context->undoCount()==beforeMerge+1,"Continuous batch edits must merge until release");
    session.edit("history.end-edit");
    session.edit("component.batch-field",{{"nodes",{"a","b"}},{"type","azure.transform"},{"field","translation"},{"axis",2},{"reset",true}});
    require(context->scene().nodes[0].translation[2]==0&&context->scene().nodes[1].translation[2]==0,"Per-axis reset must consume registry defaults");
    auto probe=reflection::reflectedType<EditorProbe>("tool.editor-probe",1,{
        reflection::withEditor(reflection::property("weight","Weight",&EditorProbe::weight,0,10),"kg",2,true),
        reflection::withMetadata(reflection::property("locked","Locked",&EditorProbe::locked,0,10),"","",true,true),
        reflection::withEditor(reflection::property("single","Single",&EditorProbe::single,0,10),"",2,false)});
    runtimeComponentRegistry().registerComponent<EditorProbe>(std::move(probe));
    for(const auto* id:{"a","b"}){session.selection().set({id});require(static_cast<bool>(session.edit("component.add",{{"type","tool.editor-probe"}})),"Registered extension must support component authoring");}
    session.selection().set({"a","b"});
    require(!session.edit("component.batch-field",{{"nodes",{"a","b"}},{"type","tool.editor-probe"},{"field","locked"},{"value",4}}),"Read-only fields must reject bulk edits");
    require(!session.edit("component.batch-field",{{"nodes",{"a","b"}},{"type","tool.editor-probe"},{"field","single"},{"value",4}}),"Batch policy must reject multi-owner writes");
    require(static_cast<bool>(session.edit("component.batch-field",{{"nodes",{"a","b"}},{"type","tool.editor-probe"},{"field","weight"},{"value",4}})),"Registered extension must share batch editing");
    require(context->componentData("a","tool.editor-probe").at("weight")==4,"Extension property must be updated");
    require(PropertyEditorRegistry::selection(*context,{"a","b"},"tool.editor-probe").at("weight").at("unit")=="kg","Extension units must flow into property presentation");
    session.selection().set({});require(PropertyEditorRegistry::selection(*context,{},"azure.transform").empty(),"Empty selection must have no editable fields");
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
