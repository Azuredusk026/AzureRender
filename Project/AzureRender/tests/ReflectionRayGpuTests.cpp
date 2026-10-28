#include "render/ReflectionRay.hpp"
#include "render/LinearMath.hpp"
#include "resources/BinaryFile.hpp"
#include "support/VulkanComputeFixture.hpp"
#include "support/ComputeTexture.hpp"
#include <cstring>
using namespace azurerender;
namespace {
void require(bool v,const char* message){if(!v)throw std::runtime_error(message);}
constexpr unsigned width=65,height=33;
void fixture(test::VulkanComputeFixture& host,const ReflectionTraceShaders& rayShaders,const ReflectionPyramidShaders& pyramidShaders,
    const std::vector<char>& filterShader,bool packed,unsigned mode){
    const bool ortho=mode==1||mode==3,rolled=mode==2,moved=mode==4;
    ReflectionTraceView view;view.right=rolled?std::array<float,3>{0,1,0}:std::array<float,3>{1,0,0};
    view.up=rolled?std::array<float,3>{-1,0,0}:std::array<float,3>{0,1,0};view.forward={0,0,-1};
    view.projection=ortho?internal::orthographic(-3,3,-2,2,.1F,100.F):internal::perspective(1.57079632679F,float(width)/height,.1F,100.F);
    if(mode==3){view.projection[12]=-.05F;view.projection[13]=.02F;}
    view.worldToClip=internal::multiply(view.projection,internal::lookAt({0,0,0},view.forward,view.up));
    std::vector<float> surfaces,colors;
    constexpr float nz=.8660254037844386F;
    const float octX=.5F/(.5F+nz);
    for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){
        const float u=(float(x)+.5F)/width*2-1,v=(float(y)+.5F)/height*2-1;
        const float a=(u+(ortho?-view.projection[12]:view.projection[8]))/view.projection[0];
        const float b=(v+(ortho?-view.projection[13]:view.projection[9]))/view.projection[5];
        const float cameraX=view.right[0]*a+view.up[0]*b;
        float depth=ortho?3+.5F/nz*cameraX:3/(1-.5F/nz*cameraX);
        if(depth<=0)depth=1000000;
        const float wallX=ortho?cameraX:cameraX*2;
        const bool wall=wallX>=(moved?2.5F:1.5F)&&depth>2;
        surfaces.insert(surfaces.end(),wall?std::initializer_list<float>{2,.5F,.5F,1}:std::initializer_list<float>{depth,octX*.5F+.5F,.5F,0});
        colors.insert(colors.end(),wall?std::initializer_list<float>{1,.2F,.1F,1}:std::initializer_list<float>{0,0,0,1});
    }
    test::ComputeTexture surface(*host.backend,width,height,1,4,VK_FORMAT_R32G32B32A32_SFLOAT,surfaces);
    test::ComputeTexture color(*host.backend,width,height,1,4,VK_FORMAT_R32G32B32A32_SFLOAT,colors);
    ReflectionSignalPyramidGpu signals(*host.backend,{width,height},1,2,packed?ReflectionPyramidPrecision::RequirePacked:ReflectionPyramidPrecision::Float32,pyramidShaders);
    const auto formats=signals.base(0);ReflectionRayGpu ray(*host.backend,{width,height},2,formats.colorFormat,formats.confidenceFormat,rayShaders);
    ReflectionSpatialFilterGpu filter(*host.backend,{width,height},2,filterShader);
    std::array<rhi::GpuBuffer,2> buffers{};
    try{
        for(auto& buffer:buffers)buffer=host.allocator.createBuffer(width*height*16,VK_BUFFER_USAGE_TRANSFER_DST_BIT,true);
        host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer){
            for(unsigned slot=0;slot<2;++slot){signals.beginBaseWrite(commands,slot);ScreenSpaceReflectionSettings settings;
                settings.enabled=slot==0;settings.maxDistance=12;settings.maxSteps=128;
                ray.record(commands,slot,{surface.view,color.view,width,height},signals.base(slot),view,settings);}
            for(unsigned slot=0;slot<2;++slot){signals.record(commands,slot,surface.view,1,{});const auto signal=signals.output(slot);
                ReflectionFilterInputViews input{surface.view,surface.view,signal.color,width,height,1,1};input.confidence=signal.confidence;input.confidenceLevels=1;input.roughnessChannel=3;
                ReflectionFilterParameters parameters;parameters.maxLod=0;filter.record(commands,slot,input,parameters);
                const auto output=filter.output(slot);
                commands.imageBarrier({output.image,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,1,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT});
                commands.copyImageToBuffer(output.image,buffers[slot].buffer,{width,height});
                commands.imageBarrier({output.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,1,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_ACCESS_TRANSFER_READ_BIT,VK_ACCESS_SHADER_READ_BIT});
                commands.bufferBarrier({buffers[slot].buffer,0,buffers[slot].size,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT});}
        });
        for(unsigned slot=0;slot<2;++slot){auto& buffer=buffers[slot];host.allocator.invalidate(buffer,0,buffer.size);const auto* data=static_cast<const float*>(buffer.mapped);
            for(unsigned i=0;i<width*height*4;++i)require(std::isfinite(data[i]),"Nonfinite reflection ray/filter output");
            const auto* center=data+((height/2)*width+width/2)*4;
            std::cout<<"Trace packed="<<packed<<" mode="<<mode<<" slot="<<slot<<" center="<<center[0]<<','<<center[1]<<','<<center[2]<<','<<center[3]<<'\n';
            if(slot||moved)require(center[0]==0&&center[3]==0,"Disabled or moved reflector must have no central ray hit");
            else {require(center[3]>.5F,"Analytic mirror must hit the visible emissive wall");const std::array<float,3> expected{1,.2F,.1F};
                for(unsigned c=0;c<3;++c)require(std::abs(center[c]-expected[c])<(packed?.02F:1e-5F),"Reflected radiance differs from independent wall color");}
        }
        for(auto& buffer:buffers)host.allocator.destroyBuffer(buffer);
    }catch(...){for(auto& buffer:buffers)host.allocator.destroyBuffer(buffer);throw;}
}
}
int main(int argc,char** argv)try{
    require(argc==6,"Expected packed/wide ray and pyramid shaders, then filter shader");test::VulkanComputeFixture host;
    const ReflectionTraceShaders rays{readBinaryFile(argv[1]),readBinaryFile(argv[2])};
    const ReflectionPyramidShaders pyramids{readBinaryFile(argv[3]),readBinaryFile(argv[4])};const auto filter=readBinaryFile(argv[5]);
    for(bool packed:{false,true})for(unsigned mode=0;mode<5;++mode)fixture(host,rays,pyramids,filter,packed,mode);
    require(host.errors==0&&!host.allocator.statistics().liveImages&&!host.allocator.statistics().liveBuffers,"Reflection ray validation or retirement failed");
    std::cout<<"PASS: 20 outputs, perspective/orthographic/rolled/offset/motion, validation=0 resources=0\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
