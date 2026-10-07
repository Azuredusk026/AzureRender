#include "editor/PanelContext.hpp"
#include "editor/EditorSession.hpp"
#include "editor/EditorWorkspace.hpp"
#include "extensions/ExtensionRegistry.hpp"
#include <iostream>
using namespace azurerender;
void check(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
struct MinimalPanel final:IEditorPanel {
    std::string_view id() const noexcept override{return "extension.selection";}
    std::string_view title() const noexcept override{return "Selection observer";}
    void draw(PanelContext& context) override{
        check(context.document().nodes.size()==2,"Read-only scene view");
        context.selection().set({"second"});
        check(context.selection().selected()==std::vector<std::string>{"second"},"Selection shared through production service");
    }
};
int main(){try{
    SceneDocument scene;SceneNode first;first.id="first";SceneNode second;second.id="second";
    scene.nodes={first,second};auto document=std::make_shared<EditorContext>(scene,std::filesystem::temp_directory_path()/"azure panel context.azscene");
    EditorSession session(document);auto panelContext=session.panelContext();
    auto& registry=session.panelRegistry();registry.registerFactory({"extension.selection",1,{"editor.panel"},{}},[]{return std::make_unique<MinimalPanel>();});
    auto panel=registry.create("extension.selection");panel->draw(panelContext);
    check(document->selectedNode()->id=="second","Cross-panel primary identity");
    const auto version=session.edits().version();bool failed=false;
    try{panelContext.selection().set({"missing"});}catch(const std::exception&){failed=true;}
    check(failed&&session.edits().version()==version,"Unknown identity preserves selection");
    session.edit("node.delete");check(panelContext.selection().selected()==std::vector<std::string>{"first"},"Delete resolves surviving identity");
    session.edit("history.undo");check(panelContext.selection().selected()==std::vector<std::string>{"second"},"Undo restores stable identity");
    panelContext.selection().set({});check(!document->selectedNode(),"Empty selection shared");
    EditorWorkspace workspace;workspace.registerPanel(std::string(panel->id()),std::string(panel->title()),false);
    check(!workspace.visible("extension.selection"),"External panel default visibility");
    workspace.setVisible("extension.selection",true);workspace.reset();
    check(!workspace.visible("extension.selection"),"Reset retains registered panel declaration");
    const auto folder=std::filesystem::temp_directory_path()/"azure extension workspace";
    workspace.setVisible("extension.selection",true);workspace.save(folder);
    EditorWorkspace reopenedWorkspace;reopenedWorkspace.registerPanel("extension.selection","Selection observer",false);
    check(reopenedWorkspace.load(folder)&&reopenedWorkspace.visible("extension.selection"),"Registered extension persists in a new workspace");
    std::filesystem::remove_all(folder);
    const auto beforeSettings=session.edits().version();
    check(static_cast<bool>(session.edit("settings.set",{{"name","editor.scale"},{"value",1.5},{"source","user-file"}})),"Settings production operation");
    check(session.settings().get("editor.scale")==1.0,"Panel setting waits for boundary");session.settings().applyPending();
    check(session.settings().get("editor.scale")==1.5,"Panel source consumed");
    check(!session.edit("settings.set",{{"name","editor.scale"},{"value",4.0}}),"Settings operation rejects bad range");
    check(session.edits().version()==beforeSettings,"Preferences keep project version unchanged");
    std::cout<<"Minimal panel context and shared identity passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
