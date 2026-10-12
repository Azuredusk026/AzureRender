#include "render/Renderer2D.hpp"
#include "rhi/NullRhi.hpp"
#include <iostream>
#include <fstream>
using namespace azurerender;
int main(int argc,char** argv){if(argc!=2)return 2;try{
    auto check=[](bool value,const char* message){if(!value)throw std::runtime_error(message);};
    rhi::NullRhi rhi;
    {GameUiRenderer backend(rhi,VK_NULL_HANDLE,argv[1]);backend.beginFrame(1,{640,360});Renderer2D renderer(backend);
        renderer.sprite({0,0,32,32},{1,0,0,1},0,2);renderer.sprite({40,0,32,32},{0,1,0,1},0,2);
        renderer.line({0,40},{100,40},4,{0,0,1,1},1);
        const auto result=renderer.flush();check(result.primitives==3&&result.batches==1&&result.vertices==12&&result.indices==18,"Compatible layer-ordered primitives must batch");
        rhi::NullCommandRecorder commands;backend.record(commands);check(backend.drawCalls()==1,"2D output must reach the render backend");
        backend.beginFrame(4,{640,360});check(rhi.allocator().statistics().liveBuffers==0,"Transient 2D geometry must retire after in-flight frames");
        const auto imagePath=std::filesystem::temp_directory_path()/std::filesystem::u8path(u8"azure-二维精灵.svg");
        {std::ofstream file(imagePath);file<<"<svg width=\"2\" height=\"2\"><rect width=\"2\" height=\"2\" fill=\"red\"/></svg>";}
        Rml::Vector2i dimensions;const auto texture=backend.LoadTexture(dimensions,imagePath.u8string());std::filesystem::remove(imagePath);
        check(texture&&dimensions.x==2&&dimensions.y==2,"2D sprite loading must share the image codec and Unicode path support");
        renderer.sprite({0,0,16,16},{1,1,1,1},texture);renderer.sprite({20,0,16,16},{1,1,1,1});
        check(renderer.flush().batches==2,"Different texture bindings must preserve separate ordered batches");backend.ReleaseTexture(texture);
        renderer.sprite({0,0,16,16},{1,1,1,1});check(renderer.flush().primitives==1,"Renderer must support a separate label workflow");
        bool invalid=false;try{renderer.sprite({0,0,-1,1},{1,1,1,1});}catch(const std::exception&){invalid=true;}check(invalid,"Invalid sprite geometry must be rejected");
        const auto label=Renderer2D::projectLabel({0,0,.5F,1},{640,360});check(label&&(*label)[0]==320&&(*label)[1]==180,"World labels must project to viewport coordinates");
        check(!Renderer2D::projectLabel({0,0,0,-1},{640,360}),"Labels behind the camera must be clipped");
    }
    check(rhi.allocator().statistics().liveBuffers==0&&rhi.allocator().statistics().liveImages==0,"2D teardown must release backend resources");
    std::cout<<"2D primitives, batching, projection and retirement passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
