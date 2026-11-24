#include "runtime/GameUi.hpp"
#include "foundation/NativeFilePath.hpp"
#include <RmlUi/Core/RenderInterface.h>
#include <iostream>
#include <stdexcept>
#include <filesystem>
#include <fstream>
struct TestRenderer: Rml::RenderInterface {
    Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex>,Rml::Span<const int>)override{return ++next;}
    void RenderGeometry(Rml::CompiledGeometryHandle,Rml::Vector2f,Rml::TextureHandle)override{}
    void ReleaseGeometry(Rml::CompiledGeometryHandle)override{}
    Rml::TextureHandle LoadTexture(Rml::Vector2i& size,const Rml::String& source)override{if(!std::filesystem::is_regular_file(azurerender::nativeFilePath(std::filesystem::u8path(source))))return 0;lastImage=source;size={16,16};++imageLoads;return ++next;}
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
  const auto folder=std::filesystem::temp_directory_path()/std::filesystem::u8path(u8"azure-ui 中文 🌟 relative-image");std::filesystem::create_directories(folder);
  const auto font=folder/"font.ttf";std::filesystem::copy_file(argv[2],font,std::filesystem::copy_options::overwrite_existing);
  const auto image=folder/std::filesystem::u8path(u8"精灵.svg");std::ofstream(image)<<"<svg width='16' height='16'/>";
  std::ofstream(folder/"scene.azure2d")<<u8R"({"schemaVersion":1,"size":[640,360],"items":[{"id":"tile","kind":"sprite","rect":[0,0,16,16],"image":"精灵.svg"}]})";
  {azurerender::GameUi sprites(folder/"scene.azure2d",font,renderer);sprites.update(.016);sprites.render();}
  if(renderer.imageLoads!=1||std::filesystem::u8path(renderer.lastImage)!=image)throw std::runtime_error("Windows 2D documents must resolve relative Unicode textures against their own directory");
  const auto form=folder/"notes.rml";
  std::ofstream(form)<<"<rml><head></head><body><div id='note'><textarea id='body'></textarea></div><img id='preview' width='16' height='16'/></body></rml>";
  {
   azurerender::GameUi notes(form,font,renderer);notes.resize(640,360,1);notes.update(.016);
   const auto replacement=azurerender::nativeFilePath(folder/std::string(80,'p')/std::filesystem::u8path(u8"本机 图片.svg"));
   std::filesystem::create_directories(replacement.parent_path());std::ofstream(replacement)<<"<svg width='16' height='16'/>";
   const auto before=renderer.imageLoads;
   notes.setImage("preview",azurerender::nativeFilePath(replacement));notes.update(.016);notes.render();
   if(renderer.imageLoads!=before+1||azurerender::nativeFilePath(std::filesystem::weakly_canonical(std::filesystem::u8path(renderer.lastImage)))!=azurerender::nativeFilePath(std::filesystem::weakly_canonical(replacement)))
       throw std::runtime_error("Dynamic images must resolve native Unicode paths");
   bool wrongImage=false;try{notes.setImage("note",replacement);}catch(const std::invalid_argument&){wrongImage=true;}
   if(!wrongImage)throw std::runtime_error("Image binding must reject ordinary elements");
   notes.setImage("preview",{});notes.update(.016);notes.render();
   notes.setFormValue("body",u8"笔记\n<&>");
   if(notes.formValue("body")!=u8"笔记\n<&>")throw std::runtime_error("UI UTF-8 form text changed");
   notes.setFormValue("body","");
   const auto field=notes.bounds("body");
   const auto fieldX=static_cast<int>(field[0]+field[2]/2);
   const auto fieldY=static_cast<int>(field[1]+field[3]/2);
   notes.pointer(fieldX,fieldY,true);notes.pointer(fieldX,fieldY,false);
   if(!notes.wantsKeyboard())throw std::runtime_error("UI textarea did not receive focus");
   notes.character(U'一');notes.key(257,true);notes.key(257,false);notes.character(U'二');
   if(notes.formValue("body")!=u8"一\n二")throw std::runtime_error("Enter did not insert a line break");
   notes.setVisible("note",false);notes.update(.016);
   if(notes.wantsPointer(12,12))throw std::runtime_error("Hidden form receives pointer input");
   notes.setVisible("note",true);notes.update(.016);
   bool wrongElement=false;try{notes.formValue("note");}catch(const std::invalid_argument&){wrongElement=true;}
   if(!wrongElement)throw std::runtime_error("UI form binding must reject ordinary elements");
  }
  std::filesystem::remove_all(folder);
  if(renderer.imageLoads!=2)throw std::runtime_error("Relative and dynamic Unicode images must both reach the renderer");
  std::cout<<"RmlUi document, text binding, button action, DPI resize and invalid element handling passed\n";
 }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
