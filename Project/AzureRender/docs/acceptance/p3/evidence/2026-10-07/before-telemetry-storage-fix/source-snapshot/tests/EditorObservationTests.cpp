#include "editor/EditorAutomation.hpp"
#include <fstream>
#include <iostream>
#include <thread>
using namespace azurerender;
using Json=nlohmann::json;
void check(bool value,const char* message) { if(!value)throw std::runtime_error(message); }
template<class F> void rejects(F f) { bool bad=false;try{f();}catch(const std::exception&){bad=true;}check(bad,"Expected query rejection"); }
int main() { const auto path=std::filesystem::temp_directory_path()/"azure editor observation.json";try {
    {std::ofstream file(path);file<<Json::array({{{"frame",1},{"command","wait-until"},{"name","ready"},{"equals",true},{"timeoutMs",1000}},{{"frame",1},{"command","node"},{"id","after-ready"}}});}
    auto context=std::make_shared<EditorContext>(SceneDocument{},path.parent_path()/"azure observation scene.azscene");EditorSession session(context);bool ready=false;
    EditorAutomation automation(path);check(automation.needsObservations(),"Discover conditional observation needs");
    automation.setObservations([&](const std::string& name)->Json { if(name!="ready")throw std::invalid_argument("Unknown query");return ready; });
    automation.advance(1,session);automation.advance(100,session);check(context->scene().nodes.empty(),"Frame count cannot substitute for readiness");
    ready=true;automation.advance(101,session);check(automation.complete()&&context->scene().nodes.size()==1,"Readiness gates production operation");
    {std::ofstream file(path);file<<Json::array({{{"frame",0},{"command","wait-until"},{"name","ready"},{"equals",false},{"timeoutMs",1}}});}
    EditorAutomation timeout(path);timeout.setObservations([](const auto&)->Json{return true;});timeout.advance(1,session);
    std::this_thread::sleep_for(std::chrono::milliseconds(2));rejects([&]{timeout.advance(2,session);});
    check(!timeout.report().at("editorActions").at(0).at("passed"),"Wait failure recorded");
    {std::ofstream file(path);file<<Json::array({{{"frame",0},{"command","assert-query"},{"name","missing"},{"equals",true}}});}
    EditorAutomation unknown(path);unknown.setObservations([](const auto&)->Json {throw std::invalid_argument("Unknown query");});rejects([&]{unknown.advance(1,session);});
    check(!unknown.report().at("editorActions").at(0).at("passed"),"Unknown query recorded");
    const auto before=session.edits().version();
    const auto generated=session.edit("asset.generate",{{"generator","azure.text"},{"output","assets:/candidate.json"},
        {"parameters",{{"source","{}"}}},{"license","LicenseRef-Project"}});
    check(generated.status==EditStatus::Rejected&&session.edits().version()==before,
        "Generation without a project must reject and preserve the document");
    std::filesystem::remove(path);std::cout<<"Editor conditional observation gates passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';std::filesystem::remove(path);return 1;} }
