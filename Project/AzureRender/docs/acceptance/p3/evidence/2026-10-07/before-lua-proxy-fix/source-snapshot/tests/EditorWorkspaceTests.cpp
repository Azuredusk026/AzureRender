#include "editor/EditorWorkspace.hpp"
#include <iostream>
#include <cmath>
#include "editor/EditorCameraController.hpp"
#include <fstream>
#include <stdexcept>
using namespace azurerender;
void check(bool value,const char* text){if(!value)throw std::runtime_error(text);}
int main(){try{
 EditorViewportInput focus;focus.frameRequested=true;focus.frameTarget={4,2,-3};
 std::array<float,3> camera={0,0,0},target={0,0,0};
 check(EditorCameraController::apply(focus,camera,target),"Focus command must move camera");
 check(target[0]==4 && target[2]==-3 && camera[2]>target[2],"Focus selected world coordinates");
 EditorWorkspace workspace;
 check(workspace.panels().size()==10,"Nine standard panels and settings extension registered");
 check(!workspace.visible("settings"),"Settings extension is closed by default");
 workspace.setVisible("settings",true);
 for(float dpi:{1.F,1.5F,2.F})for(auto size:{std::array<float,2>{1920,1080},std::array<float,2>{1280,720}}){
  auto layout=EditorWorkspace::layout(size[0],size[1],dpi);
  check(layout.viewportWidth>=480 && layout.viewportHeight>=270,"Physical viewport budget");
 }
 auto path=std::filesystem::temp_directory_path()/"azure-workspace-unit";
 std::filesystem::create_directories(path);
 workspace.setVisible("console",false);workspace.save(path);
 EditorWorkspace restored;check(restored.load(path),"Valid workspace loads");check(!restored.visible("console"),"Panel closure persists");check(restored.visible("settings"),"Extension panel persists");
 restored.setVisible("console",true);check(restored.visible("console"),"Window menu restoration");
 std::ofstream(path/"layout.ini")<<"[Docking][Data]\nDockSpace ID=0x1234\n";
 workspace.save(path);check(restored.load(path),"Saved docking data loads");
 std::ofstream(path/"layout.ini")<<"[Docking][Data]\ncorrupt docking data\n";
 check(!restored.load(path),"Corrupt docking data rejected even with valid header");
 check(restored.visible("console"),"Corrupt docking data restores defaults");
 std::ofstream(path/"workspace.json")<<"{invalid";
 check(!restored.load(path),"Corrupt state rejected");check(restored.visible("console"),"Corrupt state uses defaults");
 std::ofstream(path/"workspace.json")<<"{\"version\":999,\"panels\":{}}";
 check(!restored.load(path),"Future version rejected");
 restored.reset();check(restored.visible("viewport"),"Reset restores viewport");
 const auto screen=EditorWorkspace::ndcToScreen(.25F,-.6F);
 check(std::abs(screen[0]-.625F)<1e-6F && std::abs(screen[1]-.2F)<1e-6F,"Vulkan positive viewport Y");
 std::filesystem::remove(path/"workspace.json");std::filesystem::remove(path/"layout.ini");std::filesystem::remove(path);
 std::cout<<"Editor workspace: passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
