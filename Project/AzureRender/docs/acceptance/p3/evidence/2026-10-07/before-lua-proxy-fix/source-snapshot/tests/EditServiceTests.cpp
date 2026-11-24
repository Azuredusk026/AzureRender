#include "editor/EditorSession.hpp"
#include "editor/EditorAutomation.hpp"
#include "editor/commands/EditService.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
using namespace azurerender;
void require(bool value,const char* message) { if(!value)throw std::runtime_error(message); }
EditRequest request(EditService& edits,const std::string& id,nlohmann::json parameters) {
    return {"service-test",id,std::move(parameters),edits.version(),{}};
}
int main() {
    const auto root=std::filesystem::temp_directory_path()/"azure-edit-service-test";
    try {
        std::filesystem::create_directories(root);
        auto document=SceneDocument::fromAsset("test.gltf");
        auto first=std::make_shared<EditorContext>(document,root/"first.azscene");
        auto second=std::make_shared<EditorContext>(document,root/"second.azscene");
        EditorSession menu(first),automatic(second);
        const auto descriptions=menu.edits().describe();
        require(descriptions.is_array()&&descriptions.size()>=20,"Production edit capabilities must be discoverable");
        for(const auto& descriptor:descriptions)
            require(descriptor.at("version")==1&&descriptor.at("parameters").is_object(),
                    "Each operation must describe its stable version and parameter contract");
        auto result=menu.edits().execute(request(menu.edits(),"node.rename",{{"value","Shared operation"}}));
        require(result.status==EditStatus::Applied,"A valid edit must apply through the service");
        std::ofstream(root/"actions.json")<<R"([{"frame":0,"command":"rename","value":"Shared operation"}])";
        EditorAutomation replay(root/"actions.json");replay.advance(0,automatic);
        require(first->documentContent()==second->documentContent(),
                "UI operation and automation must produce the same document");
        require(menu.execute(EditorCommand::Undo)&&automatic.execute(EditorCommand::Undo),
                "Menu and shortcut history operations must use the registered entry");
        require(first->documentContent()==second->documentContent()&&first->selectedNode()->name==document.nodes[0].name,
                "Frontend undo must restore the same state");
        const auto before=first->documentContent();const auto version=menu.edits().version();
        require(menu.edits().execute(request(menu.edits(),"missing.operation",nlohmann::json::object())).status==EditStatus::Rejected,
                "An unknown operation must be rejected");
        for(const auto& parameters:std::vector<nlohmann::json>{
                {{"value",42}},{{"value","valid"},{"unexpected",true}},nlohmann::json::object()})
            require(menu.edits().execute(request(menu.edits(),"node.rename",parameters)).status==EditStatus::Rejected,
                    "Wrong type, unknown field and missing value must be rejected");
        require(menu.edits().execute(request(menu.edits(),"node.transform",{{"scale",{0,1,1}}})).status==EditStatus::Failed,
                "Invalid transform values must be rejected by the domain validator");
        require(first->documentContent()==before&&menu.edits().version()==version,
                "Rejected edits must retain document and revision");
        require(menu.execute(EditorCommand::Play),"Preview must start through the production operation");
        menu.edits().registry().add({"extension.play-write",1,{{"type","object"},{"properties",nlohmann::json::object()}},true,true,false},
            [](EditorContext& c,const nlohmann::json&)->nlohmann::json { c.setSelectedNodeName("Preview mutation");return nullptr; });
        require(menu.edit("extension.play-write").status==EditStatus::Rejected&&first->documentContent()==before,
                "A document-writing descriptor must be guarded even without requiresIdle");
        auto blocked=menu.edits().execute(request(menu.edits(),"node.create",{{"id","blocked"}}));
        require(blocked.status==EditStatus::Rejected&&first->documentContent()==before,
                "Document edits must be rejected while the preview is active");
        require(menu.execute(EditorCommand::Stop)&&first->documentContent()==before,
                "Stop must preserve the editing document");
        auto custom=[](EditorContext& c,const nlohmann::json&) -> nlohmann::json {
            c.setSelectedNodeName("Registered extension");return nullptr;
        };
        EditDescriptor descriptor{"extension.rename",1,{{"type","object"},{"properties",nlohmann::json::object()}},true,true,true};
        menu.edits().registry().add(descriptor,custom);automatic.edits().registry().add(descriptor,custom);
        require(static_cast<bool>(menu.edit("extension.rename")),"Registered extensions must use the normal UI service");
        std::ofstream(root/"extension.json")<<R"([{"frame":0,"command":"edit","operation":"extension.rename"}])";
        EditorAutomation extension(root/"extension.json");extension.advance(0,automatic);
        require(first->documentContent()==second->documentContent(),"Automation must discover and execute the same registered extension");
        const auto stable=first->documentContent();const auto stableVersion=menu.edits().version();
        const auto undo=first->undoCount(),redo=first->redoCount();const auto dirty=first->dirty();
        menu.edits().registry().add({"extension.failure",1,descriptor.parameters,true,false,true},
            [](EditorContext& c,const nlohmann::json&)->nlohmann::json {
                c.setSelectedNodeName("Partial failure");throw std::runtime_error("Failure after mutation");
            });
        require(menu.edit("extension.failure").status==EditStatus::Failed,"A handler failure must be observable");
        require(first->documentContent()==stable&&menu.edits().version()==stableVersion&&first->undoCount()==undo&&first->redoCount()==redo&&first->dirty()==dirty,
                "A failed single operation must retain document, version, dirty state and history");
        const auto renderProposal=request(menu.edits(),"node.rename",{{"value","obsolete-render"}});
        first->renderSettings().blackhole.quality=BlackholeQuality::Performance;
        require(menu.edits().execute(renderProposal).status==EditStatus::Stale,
                "The content fingerprint must include all render settings");
        const auto valid=first->documentContent();const auto validVersion=menu.edits().version();
        menu.edits().registry().add({"extension.invalid",1,descriptor.parameters,true,true,true},
            [](EditorContext& c,const nlohmann::json&)->nlohmann::json { c.scene().nodes.push_back(SceneNode{});return nullptr; });
        require(menu.edit("extension.invalid").status==EditStatus::Failed&&first->documentContent()==valid&&menu.edits().version()==validVersion,
                "A registered handler must not commit a structurally invalid document");
        menu.edits().registry().add({"extension.readonly",1,descriptor.parameters,false,true,false},
            [](EditorContext& c,const nlohmann::json&)->nlohmann::json { c.setSelectedNodeName("Unauthorized mutation");return nullptr; });
        require(menu.edit("extension.readonly").status==EditStatus::Rejected&&first->documentContent()==valid,
                "Read-only operation descriptors must prohibit document mutation");
        require(static_cast<bool>(menu.edit("node.select",{{"indices",nlohmann::json::array()}}))&&!first->selectedNode(),
                "An empty selection must not retain an implicit target");
        auto empty=std::make_shared<EditorContext>(SceneDocument{},root/"empty.azscene");
        require(empty->selectedNodes().empty()&&!empty->selectedNode(),"An empty document must have an empty selection");
        require(static_cast<bool>(menu.edit("node.select",{{"index",0}})),"Restore a valid selection");
        require(static_cast<bool>(menu.edit("node.child",{{"parent",0}})),"Create a removable child");
        const auto child=first->scene().nodes.size()-1;
        require(static_cast<bool>(menu.edit("node.select",{{"index",child}})),"Select the child");
        require(static_cast<bool>(menu.edit("node.remove",{{"index",child}}))&&first->selectedNode()&&first->selectedNodeIndex()<first->scene().nodes.size(),
                "Removing a selected subtree must preserve a valid selection");
        const auto candidateDocument=first->documentContent();
        menu.edits().registry().add({"extension.zero-scale",1,descriptor.parameters,true,true,true},
            [](EditorContext& c,const nlohmann::json&)->nlohmann::json { c.scene().nodes[0].scale[0]=0;return nullptr; });
        require(menu.edit("extension.zero-scale").status==EditStatus::Failed&&first->documentContent()==candidateDocument,
                "Candidate validation must apply scale constraints to registered extensions");
        menu.edits().registry().add({"extension.orphan-light",1,descriptor.parameters,true,true,true},
            [](EditorContext& c,const nlohmann::json&)->nlohmann::json { c.scene().lights.push_back({"orphan","missing"});return nullptr; });
        require(menu.edit("extension.orphan-light").status==EditStatus::Failed&&first->documentContent()==candidateDocument,
                "Candidate validation must reject lights referencing unknown nodes");
        std::cout<<"Edit discovery, frontend consistency, validation and preview guards passed\n";
        std::filesystem::remove_all(root);
    }catch(const std::exception& error) {
        std::cerr<<error.what()<<'\n';std::filesystem::remove_all(root);return 1;
    }
}
