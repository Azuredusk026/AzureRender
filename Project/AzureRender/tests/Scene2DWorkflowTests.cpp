#include "runtime/GameUi.hpp"
#include "render/Renderer2D.hpp"
#include "rhi/NullRhi.hpp"
#include <iostream>
using namespace azurerender;
int main(int argc,char** argv){if(argc!=4)return 2;try{
    rhi::NullRhi rhi;
    {GameUiRenderer backend(rhi,VK_NULL_HANDLE,argv[1]);
        {GameUi scene(argv[2],argv[3],backend);scene.resize(640,360,1);scene.update(.016);backend.beginFrame(1,{640,360});scene.render();rhi::NullCommandRecorder commands;backend.record(commands);if(!backend.drawCalls())throw std::runtime_error("2D scene and Chinese labels must generate actual render commands");
            const auto label=Renderer2D::projectLabel({0,0,.5F,1},{640,360});if(!label)throw std::runtime_error("World label must project");scene.setText("label",u8"三维对象标签：目标位置");scene.update(.016);backend.beginFrame(2,{640,360});scene.render();backend.record(commands);if(scene.text("label").find(u8"三维对象标签")==std::string::npos||!backend.drawCalls())throw std::runtime_error("World label workflow must share Chinese text rendering");}
        backend.beginFrame(5,{640,360});
    }
    if(rhi.allocator().statistics().liveBuffers||rhi.allocator().statistics().liveImages)throw std::runtime_error("2D scene and font resources must release");
    std::cout<<"2D level, Chinese text, projected world label and resource teardown passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
