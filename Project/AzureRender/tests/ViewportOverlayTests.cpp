#include "editor/viewport/ViewportOverlayRegistry.hpp"
#include "editor/EditorWorkspace.hpp"
#include <chrono>
#include <iostream>
using namespace azurerender;
int main(){try{
    const auto require=[](bool value,const char* message){if(!value)throw std::runtime_error(message);};
    auto document=SceneDocument::fromAsset("overlay-fixture.gltf");document.lights.push_back({"light","root",{1,1,1},2,3,true});
    EditorContext context(document,std::filesystem::temp_directory_path()/"azure-overlay-fixture.azscene");EditorViewDescriptor view;
    auto registry=ViewportOverlayRegistry::builtins();
    const ViewportOverlayRequest request{context,view};
    require(registry.collect(request,{{"grid","all"}},{}).size()==82,"Grid helpers must have a bounded line count");
    require(registry.collect(request,{{"light","selected"}},{}).empty(),"Selected helper policy must exclude unselected objects");
    require(registry.collect(request,{{"light","selected"}},{"root"}).size()==96,"Selected lights must expose their actual range geometry");
    registry.add("fixture","Independent helper",[](const auto&){return std::vector<ViewportOverlayLine>{{{0,0,0},{1,0,0},{1,1,1,1},"other"}};});
    require(registry.collect(request,{{"fixture","all"}},{}).size()==1,"New helper types must compose through registration");
    bool duplicate=false;try{registry.add("fixture","Duplicate",[](const auto&){return std::vector<ViewportOverlayLine>{};});}catch(const std::invalid_argument&){duplicate=true;}require(duplicate,"Duplicate provider identities must be rejected");
    registry.add("oversized","Oversized",[](const auto&){return std::vector<ViewportOverlayLine>(4097,{{0,0,0},{1,0,0},{1,1,1,1},{}});});
    bool bounded=false;try{registry.collect(request,{{"oversized","all"}},{});}catch(const std::runtime_error&){bounded=true;}require(bounded,"Unbounded helper output must fail the declared line budget");
    const auto directory=std::filesystem::temp_directory_path()/("azure-overlay-workspace-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    EditorWorkspace workspace;workspace.overlayPolicies["grid"]="hidden";workspace.save(directory);EditorWorkspace reopened;require(reopened.load(directory)&&reopened.overlayPolicies.at("grid")=="hidden","Helper visibility must persist in the user workspace");std::filesystem::remove_all(directory);
    std::cout<<"Built-in helpers, selected policies, registration, budgets and persistence passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
