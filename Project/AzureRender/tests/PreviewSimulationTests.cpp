#include "editor/EditorSession.hpp"
#include <iostream>
#include <stdexcept>
#include <fstream>
#include <chrono>
using namespace azurerender;
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
int main(){try{
    SceneDocument scene;SceneNode node;node.id="body";node.translation={0,4,0};scene.nodes.push_back(node);
    auto context=std::make_shared<EditorContext>(scene,std::filesystem::temp_directory_path()/"simulation.azscene");
    EditorSession session(context);const auto before=context->documentContent();
    require(static_cast<bool>(session.edit("preview.simulate")),"Independent physics simulation must start");
    require(session.playing()&&session.game()&&!session.scripts()&&!session.presentation(),"Simulation assembles only physics");
    require(!session.game()->hasCamera(),"Simulation keeps the editor camera");
    game::RigidBody falling;falling.dynamic=true;session.runtime()->world().addComponent(session.runtime()->entity("body"),falling);
    session.advance(.1);require(session.viewScene().nodes[0].translation[1]<3.99F,"Simulation actually advances gravity");
    require(static_cast<bool>(session.edit("viewport.orientation",{{"axis","+Y"}})),"Editor navigation remains available during simulation");
    require(session.execute(EditorCommand::Pause),"Pause simulation");
    session.advance(.1);const auto count=session.game()->steps();
    require(session.execute(EditorCommand::Step),"Single step simulation");session.advance(.1);
    require(session.game()->steps()==count+1,"Single step advances exactly one fixed step");
    require(!session.edit("node.rename",{{"value","runtime"}}),"Simulation blocks document mutation");
    require(session.execute(EditorCommand::Stop),"Stop simulation");
    require(context->documentContent()==before,"Stop preserves the editable document");
    for(int i=0;i<6;++i){require(static_cast<bool>(session.edit("preview.simulate")),"Repeated simulation start");session.advance(.02);require(session.execute(EditorCommand::Stop),"Repeated simulation stop");}
    const auto folder=std::filesystem::temp_directory_path()/("azure-simulation-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Project::create(folder,"Physics tool");Json project;{std::ifstream input(folder/"project.azureproject");input>>project;}project["startupScene"]="assets:/physics.azurelevel";std::ofstream(folder/"project.azureproject")<<project.dump();
    std::ofstream(folder/"assets/physics.azurelevel")<<Json{{"schemaVersion",1},{"id","physics"},{"sceneType","sample"},{"resources",Json::array()},{"lights",Json::array()},
        {"nodes",Json::array({{{"id","fixed"},{"components",{{"azure.rigid-body",{{"type","azure.rigid-body"},{"version",1},{"data",Json::object()}}}}}},{{"id","moving"},{"components",Json::object()}},{{"id","joint"},{"components",Json::object()}}})}}.dump();
    {
        auto author=EditorContext::openProject(folder/"project.azureproject");EditorSession authoring(author);
        author->selectNode(0);require(static_cast<bool>(authoring.edit("component.fields",{{"type","azure.rigid-body"},{"values",{{"halfExtent",{.7,.5,.5}}}}})),"Edit legacy body");
        require(author->runtimeComponents().at("fixed").at("azure.rigid-body").at("version")==3,"Editing a legacy component writes its current version");
        author->selectNode(2);require(static_cast<bool>(authoring.edit("component.add",{{"type","azure.hinge"}})),"Add hinge");
        require(static_cast<bool>(authoring.edit("component.fields",{{"type","azure.hinge"},{"values",{{"bodyA","fixed"},{"bodyB","moving"},{"axis",{0,0,1}},{"normal",{1,0,0}}}}})),"Configure hinge atomically");
        RenderSettings attached=author->renderSettings();author->attachRenderSettings(attached);const auto invalidStart=author->documentContent();
        require(!authoring.edit("preview.simulate"),"Simulation startup rejects a nonbody endpoint");
        require(!authoring.playing()&&author->documentContent()==invalidStart&&&author->renderSettings()==&attached,"Failed startup preserves document and render-settings attachment");
        const auto undo=author->undoCount();
        require(!authoring.edit("component.fields",{{"type","azure.hinge"},{"values",{{"axis",{0,0,0}}}}}),"Invalid authoring must fail");
        require(author->documentContent()==invalidStart&&author->undoCount()==undo,"Failed authoring restores document and history");
        author->selectNode(1);require(static_cast<bool>(authoring.edit("component.add",{{"type","azure.rigid-body"}})),"Add moving body");
        require(static_cast<bool>(authoring.edit("component.fields",{{"type","azure.rigid-body"},{"values",{{"dynamic",true}}}})),"Configure dynamic body");
        require(author->runtimeComponents().at("moving").at("azure.rigid-body").at("version")==3,"Edited rigid bodies use the current component version");
        author->save();auto reopened=EditorContext::openProject(folder/"project.azureproject");require(reopened->componentData("joint","azure.hinge").at("axis")==Json::array({0,0,1}),"Physics authoring saves and reopens");
        require(static_cast<bool>(authoring.edit("preview.simulate")),"Valid project simulation starts");authoring.advance(.02);require(authoring.execute(EditorCommand::Stop),"Project simulation stops");
    }
    std::filesystem::remove_all(folder);
    std::cout<<"Independent simulation, camera, pause, single step and restoration passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
