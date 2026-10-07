#define NOMINMAX
#include <windows.h>
#include <psapi.h>
#include "app/ProjectRuntimeAssembly.hpp"
#include "runtime/ScriptRuntime.hpp"
#include <fstream>
#include <iostream>
using namespace azurerender;
std::size_t memory(){PROCESS_MEMORY_COUNTERS_EX c{};c.cb=sizeof(c);if(!K32GetProcessMemoryInfo(GetCurrentProcess(),reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&c),sizeof(c)))throw std::runtime_error("Memory query failed");return c.PrivateUsage;}
int main(int argc,char** argv){
 auto root=std::filesystem::temp_directory_path()/"azure_lua_memory_diagnostic";std::filesystem::remove_all(root);
 try {
 Project::create(root,"Memory diagnostic");auto project=Project::load(root/"project.azureproject");
 const bool find=argc<2 || std::string(argv[1])=="find";
 std::ofstream(root/"assets/main.lua")<<(find?"function update(dt) for i=1,64 do local other=self:find('owner');if not other:alive() then error('missing') end end end":"function update(dt) for i=1,64 do self:get('azure.transform','translation') end end");
 AssetDatabase assets(project);assets.refresh();RuntimeLifecycle runtime;SceneDocument scene;SceneNode node;node.id="owner";scene.nodes.push_back(node);runtime.loadScene(scene);runtime.start();GameRuntime game(runtime,application::systems(),application::explorationConfiguration());
 runtime.world().addComponent(runtime.entity("owner"),game::Script{"assets:/main.lua",true});ScriptRuntime script(runtime,game,assets);
 const auto start=memory();
 for(int batch=0;batch<20;++batch){for(int i=0;i<100;++i)script.update(1.0/60.0);std::cout<<(batch+1)*6400<<" "<<memory()<<" "<<script.activeCount()<<" "<<script.errors().size()<<std::endl;}
 std::cout<<"total growth "<<memory()-start<<std::endl;
 }catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}
 std::filesystem::remove_all(root);
}
