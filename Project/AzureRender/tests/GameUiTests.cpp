#include "runtime/GameUi.hpp"
#include <RmlUi/Core/RenderInterface.h>
#include <iostream>
#include <stdexcept>
struct TestRenderer: Rml::RenderInterface {
    Rml::CompiledGeometryHandle CompileGeometry(Rml::Span<const Rml::Vertex>,Rml::Span<const int>)override{return ++next;}
    void RenderGeometry(Rml::CompiledGeometryHandle,Rml::Vector2f,Rml::TextureHandle)override{}
    void ReleaseGeometry(Rml::CompiledGeometryHandle)override{}
    Rml::TextureHandle LoadTexture(Rml::Vector2i&,const Rml::String&)override{return 0;}
    Rml::TextureHandle GenerateTexture(Rml::Span<const Rml::byte>,Rml::Vector2i)override{return ++next;}
    void ReleaseTexture(Rml::TextureHandle)override{}
    void EnableScissorRegion(bool)override{}
    void SetScissorRegion(Rml::Rectanglei)override{}
    std::uintptr_t next=0;
};
int main(int argc,char** argv){
 if(argc!=3)return 2;
 try{TestRenderer renderer;azurerender::GameUi ui(argv[1],argv[2],renderer);
  ui.resize(640,360,1);ui.setText("status","destination");ui.update(1.0/60.0);
  if(ui.text("status")!="destination")throw std::runtime_error("UI text binding failed");
  std::string action;ui.setActionHandler([&](std::string value){action=value;});
  const auto button=ui.bounds("pause");std::cout<<"button: "<<button[0]<<","<<button[1]<<","<<button[2]<<","<<button[3]<<'\n';
  const auto x=static_cast<int>(button[0]+button[2]/2),y=static_cast<int>(button[1]+button[3]/2);
  ui.pointer(x,y,true);ui.pointer(x,y,false);ui.update(1.0/60.0);
  if(action!="pause")throw std::runtime_error("UI button action failed");
  ui.resize(1280,720,2);ui.update(1.0/60.0);ui.render();
  bool rejected=false;try{ui.setText("missing","text");}catch(const std::exception&){rejected=true;}
  if(!rejected)throw std::runtime_error("Unknown UI element must be rejected");
  std::cout<<"RmlUi document, text binding, button action, DPI resize and invalid element handling passed\n";
 }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
