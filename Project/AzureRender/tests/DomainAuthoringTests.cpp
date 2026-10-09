#include "editor/EditorSession.hpp"
#include "runtime/MaterialDocument.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
using namespace azurerender;
using Json=nlohmann::json;
void require(bool v,const char* m){if(!v)throw std::runtime_error(m);}
struct Fixed: IModelTransport {
    std::string content;
    std::shared_future<ModelResponse> request(const ModelRequest&) override {std::promise<ModelResponse> p;p.set_value({true,content,{},"json"});return p.get_future().share();}
    void cancel(const std::string&) override {}
};
Json artifact(const std::string& command,Json parameters){return {{"schemaVersion",1},{"operations",Json::array({{{"command",command},{"parameters",parameters}}})}};}
int main(int argc,char** argv){if(argc!=2)return 2;
    const auto root=std::filesystem::temp_directory_path()/("azure-domains-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try{
        Project::create(root,"Domain authoring");std::filesystem::copy_file(argv[1],root/"assets/image.png");
        auto doc=EditorContext::openProject(root/"project.azureproject");EditorSession editor(doc);
        auto material=editor.edit("material.create",{{"type","azure.pbr"},{"destination","assets:/surface.azurematerial"}});
        require(static_cast<bool>(material),"Create independent material");const auto materialId=material.value.at("asset").get<std::string>();
        const auto image=doc->assets().idForPath("assets:/image.png");const auto node=doc->selectedNode()->id;
        auto transport=std::make_shared<Fixed>();ModelClient client(transport);ProposalController controller(*doc,editor.edits(),editor.generators(),client);
        const auto generate=[&](std::string domain,std::string target,Json proposal){transport->content=proposal.dump();require(controller.generate({"domain-"+domain,domain,target,"Adjust the selected content"}),"Registered domain accepts its typed target");controller.poll();if(controller.state()!=ProposalState::Ready)std::cerr<<controller.report().dump(2)<<'\n';require(controller.state()==ProposalState::Ready,"Production semantics validate the proposal");};
        generate("material",materialId,artifact("material.field",{{"asset",materialId},{"field","roughness"},{"value",.35}}));
        require(MaterialDocument::load(root/"assets/surface.azurematerial").parameters.at("roughness")==.75,"Material preview preserves source");
        require(static_cast<bool>(controller.apply(editor.edits())),"Material proposal applies");
        require(MaterialDocument::load(root/"assets/surface.azurematerial").parameters.at("roughness")==.35,"Material proposal uses document authoring semantics");
        require(static_cast<bool>(editor.edit("material.undo",{{"asset",materialId}})),"Material proposal shares material history");
        generate("material",materialId,artifact("material.field",{{"asset",materialId},{"field","roughness"},{"value",.4}}));
        require(static_cast<bool>(editor.edit("material.field",{{"asset",materialId},{"field","roughness"},{"value",.6}})),"Manual field update");
        require(!controller.apply(editor.edits())&&MaterialDocument::load(root/"assets/surface.azurematerial").parameters.at("roughness")==.6,"Source changes invalidate pending material proposals");
        generate("texture",image,artifact("texture.settings",{{"assets",Json::array({image})},{"values",{{"filter","nearest"}}}}));
        require(static_cast<bool>(controller.apply(editor.edits())),"Texture proposal applies through content transaction");
        const auto texture=editor.edit("texture.describe",{{"assets",Json::array({image})}});require(static_cast<bool>(texture)&&texture.value.dump().find("nearest")!=std::string::npos,"Texture interpretation updates");
        require(static_cast<bool>(editor.edit("content.undo")),"Texture proposal shares content undo");
        transport->content=artifact("component.fields",{{"type","azure.rigid-body"},{"values",{{"radius",2.}}}}).dump();
        require(controller.generate({"scope","physics",node,"Require target selection"}),"Physics snapshot accepts node scope");controller.poll();controller.poll();require(controller.state()==ProposalState::Error,"Physics rejects writes lacking stable target selection");
        auto physics=artifact("node.select",{{"id",node}});
        physics["operations"].push_back({{"command","component.add"},{"parameters",{{"type","azure.rigid-body"}}}});
        physics["operations"].push_back(artifact("component.fields",{{"type","azure.rigid-body"},{"values",{{"radius",2.}}}}).at("operations").at(0));
        generate("physics",node,physics);require(!doc->componentData(node,"azure.rigid-body").is_object(),"Physics preview preserves components");
        require(static_cast<bool>(controller.apply(editor.edits()))&&std::abs(doc->componentData(node,"azure.rigid-body").at("radius").get<double>()-2.)<1e-6,"Physics applies shared component transaction");
        require(static_cast<bool>(editor.edit("history.undo"))&&!doc->componentData(node,"azure.rigid-body").is_object(),"Physics proposal is one undo entry");
        transport->content=artifact("material.field",{{"asset",materialId},{"field","roughness"},{"value",5.}}).dump();
        require(controller.generate({"invalid","material",materialId,"Reject illegal parameter"}),"Invalid candidate can enter validation");controller.poll();controller.poll();require(controller.state()==ProposalState::Error,"Out of range material rejects without writing");
        std::filesystem::remove_all(root);std::cout<<"Material, texture and physics proposals, source conflicts and shared undo passed\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';std::filesystem::remove_all(root);return 1;}
}
