#include "runtime/GameUi.hpp"
#include <RmlUi/Core/RenderInterface.h>
#include <iostream>
#include <stdexcept>
#include <filesystem>
#include <fstream>
struct TestRenderer: Rml::RenderInterface {
    Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex>,Rml::Span<const int>)override{return ++next;}
    void RenderGeometry(Rml::CompiledGeometryHandle,Rml::Vector2f,Rml::TextureHandle)override{}
    void ReleaseGeometry(Rml::CompiledGeometryHandle)override{}
    Rml::TextureHandle LoadTexture(Rml::Vector2i& size,const Rml::String& source)override{if(!std::filesystem::is_regular_file(std::filesystem::u8path(source)))return 0;lastImage=source;size={16,16};++imageLoads;return ++next;}
    Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte>,Rml::Vector2i)override{return ++next;}
    void ReleaseTexture(Rml::TextureHandle)override{}
    void EnableScissorRegion(bool)override{}
    void SetScissorRegion(Rml::Rectanglei)override{}
    std::uintptr_t next=0;
    unsigned imageLoads=0;
    std::string lastImage;
};
int main(int argc,char** argv){
 if(argc!=3)return 2;
 try{TestRenderer renderer;azurerender::GameUi ui(argv[1],argv[2],renderer);
  ui.resize(640,360,1);ui.setText("status","destination");ui.update(1.0/60.0);
  if(ui.text("status")!="destination")throw std::runtime_error("UI text binding failed");
  std::string action;ui.setActionHandler([&](std::string value){action=value;});
  const auto button=ui.bounds("pause");std::cout<<"button: "<<button[0]<<","<<button[1]<<","<<button[2]<<","<<button[3]<<'\n';
  const auto x=static_cast<int>(button[0]+button[2]/2),y=static_cast<int>(button[1]+button[3]/2);
  if(!ui.wantsPointer(x,y)||ui.wantsPointer(630,350))throw std::runtime_error("UI pointer focus must be limited to interactive controls");
  ui.pointer(x,y,true);ui.pointer(x,y,false);ui.update(1.0/60.0);
  if(action!="pause")throw std::runtime_error("UI button action failed");
  ui.resize(1280,720,2);ui.update(1.0/60.0);ui.render();
  bool rejected=false;try{ui.setText("missing","text");}catch(const std::exception&){rejected=true;}
  if(!rejected)throw std::runtime_error("Unknown UI element must be rejected");
  const auto folder=std::filesystem::temp_directory_path()/"azure-ui-relative-image";std::filesystem::create_directories(folder);
  const auto image=folder/std::filesystem::u8path(u8"精灵.svg");std::ofstream(image)<<"<svg width='16' height='16'/>";
  std::ofstream(folder/"scene.azure2d")<<u8R"({"schemaVersion":1,"size":[640,360],"items":[{"id":"tile","kind":"sprite","rect":[0,0,16,16],"image":"精灵.svg"}]})";
  {azurerender::GameUi sprites(folder/"scene.azure2d",argv[2],renderer);sprites.update(.016);sprites.render();}
  std::filesystem::remove_all(folder);
  if(renderer.imageLoads!=1||std::filesystem::u8path(renderer.lastImage)!=image)throw std::runtime_error("Windows 2D documents must resolve relative Unicode textures against their own directory");
  std::cout<<"RmlUi document, text binding, button action, DPI resize and invalid element handling passed\n";
 }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
