#include "render/ReflectionReprojection.hpp"
#include "resources/BinaryFile.hpp"
#include "support/VulkanComputeFixture.hpp"
#include "support/ComputeTexture.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>
using namespace azurerender;
namespace {
void fixture(test::VulkanComputeFixture& host,const std::vector<char>& shader,unsigned divisor,unsigned width=17,unsigned height=9){
    const unsigned ow=std::max(1U,width/divisor),oh=std::max(1U,height/divisor),pixels=ow*oh;
    ReflectionReprojectionGpu resource(*host.backend,{width,height},2,shader,divisor);
    for(unsigned mode=0;mode<26;++mode){
        std::vector<float> surface,motion,previous;
        const float nan=std::numeric_limits<float>::quiet_NaN();
        for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){
            float depth=2+.001F*x;if(mode==2)depth=100000;if(mode==12)depth=nan;
            surface.insert(surface.end(),{depth,.5F,.5F,.2F});
            float dx=x*.0001F,z=.95F,response=1;
            if(mode==3)response=-2;if(mode==4)response=-1;if(mode==5)response=-.5F;if(mode==6)response=2.25F;if(mode==7)response=0;
            if(mode==8)dx=2;if(mode==10)dx=nan;if(mode==14)z=.2F;if(mode==15)z=500.F/999.F;
            if(mode==16||mode==17)z=.5F;if(mode==18)dx=std::numeric_limits<float>::infinity();if(mode==19)z=-.1F;if(mode==20)response=.5F;
            if(mode==22)z=1;if(mode==23)z=0;
            if(mode>=24){dx=0;z=1-.1F/depth;}
            motion.insert(motion.end(),{dx,0,z,response});
        }
        for(unsigned i=0;i<pixels;++i){float depth=mode==9?3.F:mode==13?nan:mode==16?2.0005F:mode==17?2.0011F:2.F;
            if(mode==22||mode==23)depth=.0005F;
            previous.insert(previous.end(),{0,0,depth,mode==11?0.F:mode==21?nan:1.F});}
        test::ComputeTexture surfaces(*host.backend,width,height,1,4,VK_FORMAT_R32G32B32A32_SFLOAT,surface);
        test::ComputeTexture vectors(*host.backend,width,height,1,4,VK_FORMAT_R32G32B32A32_SFLOAT,motion);
        test::ComputeTexture history(*host.backend,ow,oh,1,4,VK_FORMAT_R32G32B32A32_SFLOAT,previous);
        ReflectionReprojectionParameters parameters;parameters.historyValid=mode!=1&&mode!=24;
        if(mode==14)parameters.previousDepthProjection={-.1F,0,0,1};
        if(mode==15)parameters.previousDepthProjection={-1000.F/999.F,-1000.F/999.F,-1,0};
        if(mode==16||mode==17){parameters.previousDepthProjection={-.25F,0,0,1};parameters.relativeDepthTolerance=0;}
        if(mode==22)parameters.previousDepthProjection={2e38F,0,-2e38F,2e-38F};
        if(mode==23)parameters.previousDepthProjection={-1,0,0,1};
        const ReflectionReprojectionInputs inputs{surfaces.view,vectors.view,parameters.historyValid?history.view:VK_NULL_HANDLE,width,height};
        std::array<rhi::GpuBuffer,2> buffers{};
        try{
            for(auto& buffer:buffers)buffer=host.allocator.createBuffer(pixels*20,VK_BUFFER_USAGE_TRANSFER_DST_BIT,true);
            host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer raw){
                for(unsigned slot=0;slot<2;++slot){auto borrowed=inputs;
                    if(mode==25)borrowed.previousSelectedMotion=resource.output(1-slot).motion;
                    resource.record(commands,slot,borrowed,parameters);const auto output=resource.output(slot);
                    const VkImage images[]={output.motionImage,output.acceptanceImage};
                    for(unsigned attachment=0;attachment<2;++attachment){
                        commands.imageBarrier({images[attachment],VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,1,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT});
                        VkBufferImageCopy copy{};copy.bufferOffset=attachment?pixels*16:0;copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.imageExtent={ow,oh,1};
                        vkCmdCopyImageToBuffer(raw,images[attachment],VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,buffers[slot].buffer,1,&copy);
                        commands.imageBarrier({images[attachment],VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,1,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_ACCESS_TRANSFER_READ_BIT,VK_ACCESS_SHADER_READ_BIT});
                    }
                    commands.bufferBarrier({buffers[slot].buffer,0,buffers[slot].size,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT});
                }
            });
            for(unsigned slot=0;slot<2;++slot){host.allocator.invalidate(buffers[slot],0,buffers[slot].size);const auto* result=static_cast<const float*>(buffers[slot].mapped);
                for(unsigned y=0;y<oh;++y)for(unsigned x=0;x<ow;++x){
                    const unsigned sourceX=((2*x+1)*width)/(2*ow),i=y*ow+x;
                    const bool usable=mode!=2&&mode!=12,moving=mode!=10&&mode!=18;
                    const float weight=mode==0||mode==14||mode==15||mode==16||mode==25?1.F:mode==5?.5F:mode==6?.75F:0.F;
                    const float expected[]{moving?(mode==8?2.F:mode>=24?0:sourceX*.0001F):0,0,usable?2+.001F*sourceX:0,usable?1.F:0,weight};
                    for(unsigned c=0;c<5;++c){const auto actual=result[c<4?i*4+c:pixels*4+i];
                        if(!std::isfinite(actual)||std::abs(actual-expected[c])>2e-6F){
                            std::cerr<<"Reprojection divisor="<<divisor<<" mode="<<mode<<" slot="<<slot<<" pixel="<<x<<','<<y<<" channel="<<c<<" actual="<<actual<<" expected="<<expected[c]<<'\n';
                            throw std::runtime_error("Reprojection differs from independent selected-source, response or depth rejection reference");
                        }
                    }
                }
            }
            for(auto& buffer:buffers)host.allocator.destroyBuffer(buffer);
        }catch(...){for(auto& buffer:buffers)host.allocator.destroyBuffer(buffer);throw;}
    }
}
}
int main(int argc,char** argv)try{
    if(argc!=2)throw std::invalid_argument("Expected reprojection shader");test::VulkanComputeFixture host;
    const auto shader=readBinaryFile(argv[1]);fixture(host,shader,1);fixture(host,shader,2);
    fixture(host,shader,2,1,9);fixture(host,shader,2,17,1);
    if(host.errors||host.allocator.statistics().liveImages||host.allocator.statistics().liveBuffers)throw std::runtime_error("Reprojection validation or retirement failed");
    std::cout<<"PASS: 208 full/half/single-axis outputs, 51220 source/depth/response/cross-slot scalars, validation=0 resources=0\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
