#include "runtime/ComponentRegistry.hpp"
#include "editor/EditorSession.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
using namespace azurerender;
namespace {
struct AssetNote { std::string text="untitled"; std::uint32_t sampleCount=3; };
void require(bool value,const char* reason) { if(!value)throw std::runtime_error(reason); }
void write(const std::filesystem::path& file,const Json& value) { std::ofstream(file)<<value.dump(2); }
}
int main() {
    const auto root=std::filesystem::temp_directory_path()/
        ("azure_registered_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        auto type=reflection::reflectedType<AssetNote>("tool.asset-note",1,{
            reflection::property<AssetNote>("text","Text",&AssetNote::text,0,0),
            reflection::property<AssetNote>("sampleCount","Samples",&AssetNote::sampleCount,0,100)});
        type.properties[1].readOnly=true;
        runtimeComponentRegistry().registerComponent<AssetNote>(std::move(type));
        Project::create(root,"Asset inspection");
        Json project;
        {std::ifstream input(root/"project.azureproject");input>>project;}
        project["startupScene"]="assets:/inspect.azurelevel";
        write(root/"project.azureproject",project);
        write(root/"assets/inspect.azurelevel",{{"schemaVersion",1},{"id","inspection"},{"sceneType","sample"},
            {"resources",Json::array()},{"lights",Json::array()},
            {"nodes",Json::array({{{"id","tool"},{"components",Json::object()}}})}});
        std::ofstream(root/"assets/inspect.lua")<<
            "function init() assert(self:has('tool.asset-note')); self:set('tool.asset-note','text','from script'); "
            "assert(self:get('tool.asset-note','text')=='from script') end";
        auto context=EditorContext::openProject(root/"project.azureproject");
        context->selectNode(0);
        context->addGameplayComponent("tool.asset-note");
        context->setComponentField("tool.asset-note","text","from editor");
        const auto before=context->levelDocument();
        bool rejected=false;
        try {context->setComponentField("tool.asset-note","sampleCount",9);}catch(const std::exception&){rejected=true;}
        require(rejected&&context->levelDocument()==before,"Read-only edit mutated the document");
        context->assets().refresh();
        context->addGameplayComponent("azure.script");
        context->setComponentField("azure.script","asset","assets:/inspect.lua");
        context->save();
        auto reopened=EditorContext::openProject(root/"project.azureproject");
        require(reopened->componentData("tool","tool.asset-note").at("text")=="from editor","Saved registered component lost content");
        EditorSession session(reopened);
        require(session.execute(EditorCommand::Play),session.lastError().c_str());
        session.advance(1.0/60);
        require(session.scripts()->errors().empty(),"Lua failed to access registered component");
        const auto entity=session.runtime()->entity("tool");
        require(session.runtime()->world().tryGet<AssetNote>(entity)->text=="from script","Lua setter bypassed registered binding");
        require(session.execute(EditorCommand::Stop),"Stop failed");
        require(reopened->componentData("tool","tool.asset-note").at("text")=="from editor","Preview mutated edit content");
        std::filesystem::remove_all(root);
        std::cout<<"Independent tool component: editor, save/reopen, Lua and preview isolation passed\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';std::filesystem::remove_all(root);return 1;}
}
