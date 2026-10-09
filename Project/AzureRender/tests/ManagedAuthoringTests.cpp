#include "editor/EditorSession.hpp"
#include <fstream>
#include <iostream>
using namespace azurerender;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(int argc,char** argv){if(argc!=3)return 2;try{
    const std::string mode=argv[1];const auto root=std::filesystem::absolute(argv[2]);
    if(mode=="create"){
        Project::create(root,"Managed template validation");
        auto context=EditorContext::openProject(root/"project.azureproject");EditorSession session(context);
        for(const auto& row:std::vector<std::pair<const char*,const char*>>{{"coreclr","CoreController"},{"nativeaot","NativeController"}}){
            require(static_cast<bool>(session.edit("script.template",{{"backend",row.first},{"path","assets:/"+std::string(row.second)+".cs"}})),"Generate registered managed backend source");
        }
    }else if(mode=="verify"){
        auto context=EditorContext::openProject(root/"project.azureproject");EditorSession session(context);
        require(session.execute(EditorCommand::Play),"Start built template project");session.advance(.05);
        require(session.scripts()&&session.scripts()->errors().empty()&&session.scripts()->activeCount()==2,"Both generated templates load through production managed service");
        for(const auto& id:{"core","native"})require(session.runtime()->world().tryGet<ecs::TransformComponent>(session.runtime()->entity(id))->translation==std::array<float,3>{13,2,3},"Generated callback executes using shared typed component bindings");
        require(session.execute(EditorCommand::Stop),"Stop preserves template scene");
    }else throw std::invalid_argument("Unknown template validation mode");
    std::cout<<"Managed authoring "<<mode<<" passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
