#include "render/ProjectedReflectionPreparation.hpp"
#include "resources/BinaryFile.hpp"
#include "support/VulkanComputeFixture.hpp"
#include "support/ComputeTexture.hpp"
#include "support/ComputeStorage.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
using namespace azurerender;
namespace {
using Normal=std::array<double,3>;
Normal normalized(Normal n){const auto length=std::sqrt(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]);for(auto& v:n)v/=length;return n;}
double sign(double v){return v>=0?1:-1;}
std::array<float,2> octZ(Normal n){const auto sum=std::abs(n[0])+std::abs(n[1])+std::abs(n[2]);for(auto& v:n)v/=sum;
    const double x=n[2]<0?(1-std::abs(n[1]))*sign(n[0]):n[0],y=n[2]<0?(1-std::abs(n[0]))*sign(n[1]):n[1];return {float(x*.5+.5),float(y*.5+.5)};}
Normal decodeY(float x,float z){Normal n{{x*2-1,1-std::abs(x*2-1)-std::abs(z*2-1),z*2-1}};
    if(n[1]<0){const auto nx=(1-std::abs(n[2]))*sign(n[0]);n[2]=(1-std::abs(n[0]))*sign(n[2]);n[0]=nx;}return normalized(n);}
struct Readback {
    rhi::IGpuAllocator& allocator;std::array<std::array<rhi::GpuBuffer,4>,2> buffers{};
    explicit Readback(rhi::IGpuAllocator& value,unsigned pixels):allocator(value){for(auto& slot:buffers)for(unsigned i=0;i<4;++i)slot[i]=allocator.createBuffer(pixels*(i==0?4:i==1?16:1),VK_BUFFER_USAGE_TRANSFER_DST_BIT,true);}
    ~Readback(){for(auto& slot:buffers)for(auto& buffer:slot)allocator.destroyBuffer(buffer);}
};
std::uint64_t checks=0;
void fixture(test::VulkanComputeFixture& host,const std::vector<char>& shader,unsigned width,unsigned height,unsigned divisor){
    auto& backend=*host.backend;const unsigned ow=std::max(1U,width/divisor),oh=std::max(1U,height/divisor),pixels=ow*oh;
    const std::array<Normal,8> normals{{{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1},normalized({1,-2,3}),normalized({-2,3,-1})}};
    std::vector<float> surface(width*height*4);std::vector<Normal> expectedNormals(width*height);
    for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){const unsigned i=y*width+x,mode=i%20;const auto normal=normals[i%normals.size()];expectedNormals[i]=normal;const auto oct=octZ(normal);
        float depth=2+.01F*x+.02F*y,roughness=mode==12?.6F:mode==13?.7F:mode==14?1.F:mode==15?.525F:.15F;
        if(mode==1)depth=0;if(mode==2)depth=-1;if(mode==3)depth=100000;if(mode==4)depth=std::numeric_limits<float>::quiet_NaN();
        if(mode==5)depth=std::numeric_limits<float>::infinity();if(mode==10)depth=.01F;if(mode==11)depth=10000;
        surface[i*4]=depth;surface[i*4+1]=mode==6?-1:oct[0];surface[i*4+2]=mode==7?2:oct[1];
        surface[i*4+3]=mode==8?std::numeric_limits<float>::quiet_NaN():mode==9?1.5F:roughness;
    }
    test::ComputeTexture source(backend,width,height,1,4,VK_FORMAT_R32G32B32A32_SFLOAT,surface);
    test::ComputeStorage confidence(backend,ow,oh,VK_FORMAT_R8_UNORM,1,std::vector<char>(pixels,0));
    ProjectedReflectionEligibilityTarget target{confidence.image.image,confidence.view,{ow,oh}};
    ProjectedReflectionPreparationGpu resource(backend,{width,height},2,shader,divisor);Readback readback(host.allocator,pixels);
    // This command is deliberately abandoned before queue submission.
    bool canceled=false;try{host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer){resource.record(commands,0,source.view,target,{10,.001F},{});canceled=true;throw std::runtime_error("cancel fixture");});}catch(const std::runtime_error&){if(!canceled)throw;}
    if(!canceled)throw std::runtime_error("Cancellation control did not execute");
    for(unsigned epoch=0;epoch<2;++epoch){
        host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer){
            for(unsigned slot=0;slot<2;++slot){ProjectedReflectionPreparationParameters parameters;parameters.maxRoughness=slot?.45F:.7F;parameters.strength=epoch?(slot?.8F:0.F):(slot?.25F:1.F);
                const std::array<float,2> depth{{slot?5.F:10.F,slot?.002F:.001F}};
                commands.imageBarrier({target.image,VK_IMAGE_LAYOUT_GENERAL,VK_IMAGE_LAYOUT_GENERAL,1,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                    VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT,VK_ACCESS_SHADER_WRITE_BIT});
                resource.record(commands,slot,source.view,target,depth,parameters);const auto result=resource.output(slot);
                const std::array<VkImage,4> images{{result.depthImage,result.normalRoughnessImage,result.maskImage,target.image}};
                for(unsigned attachment=0;attachment<4;++attachment){const auto layout=attachment==3?VK_IMAGE_LAYOUT_GENERAL:VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;auto& buffer=readback.buffers[slot][attachment];
                    commands.imageBarrier({images[attachment],layout,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,1,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,
                        VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT});
                    commands.copyImageToBuffer(images[attachment],buffer.buffer,{ow,oh});
                    commands.imageBarrier({images[attachment],VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,layout,1,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_ACCESS_TRANSFER_READ_BIT,VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT});
                    commands.bufferBarrier({buffer.buffer,0,buffer.size,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT});
                }
            }
        });
        for(unsigned slot=0;slot<2;++slot){for(auto& buffer:readback.buffers[slot])host.allocator.invalidate(buffer,0,buffer.size);
            const auto& buffers=readback.buffers[slot];const auto* depths=static_cast<const float*>(buffers[0].mapped);const auto* normal=static_cast<const float*>(buffers[1].mapped);
            const auto* masks=static_cast<const unsigned char*>(buffers[2].mapped);const auto* weights=static_cast<const unsigned char*>(buffers[3].mapped);
            for(unsigned y=0;y<oh;++y)for(unsigned x=0;x<ow;++x){const unsigned sourceIndex=((2*y+1)*height/(2*oh))*width+(2*x+1)*width/(2*ow),i=y*ow+x;const auto* texel=surface.data()+sourceIndex*4;
                const double a=slot?5:10,b=slot?.002:.001,projected=(1./texel[0]-b)/a;const float maxRoughness=slot?.45F:.7F,strength=epoch?(slot?.8F:0.F):(slot?.25F:1.F);
                bool valid=texel[0]>0&&texel[0]<100000&&projected>=0&&projected<=1;for(unsigned c=0;c<4;++c)valid=valid&&std::isfinite(texel[c]);
                for(unsigned c=1;c<4;++c)valid=valid&&texel[c]>=0&&texel[c]<=1;
                const double expectedDepth=valid?projected:0;const auto decoded=decodeY(normal[i*4],normal[i*4+1]);const auto reference=valid?expectedNormals[sourceIndex]:Normal{{0,1,0}};
                const double dot=decoded[0]*reference[0]+decoded[1]*reference[1]+decoded[2]*reference[2];
                const double t=std::clamp((double(texel[3])-.75*maxRoughness)/(.25*maxRoughness),0.,1.);const auto expectedWeight=valid?std::lround((1-t*t*(3-2*t))*strength*255):0;
                if(!std::isfinite(depths[i])||std::abs(depths[i]-expectedDepth)>2e-6||!std::isfinite(dot)||dot<.99999||normal[i*4+2]!=(valid?texel[3]:1.F)||normal[i*4+3]!=0||masks[i]!=(valid?255:0)||std::abs(long(weights[i])-expectedWeight)>1){
                    std::cerr<<"Preparation "<<width<<'x'<<height<<'/'<<divisor<<" epoch="<<epoch<<" slot="<<slot<<" pixel="<<x<<','<<y<<" depth="<<depths[i]<<" expected="<<expectedDepth<<" normalDot="<<dot<<" weight="<<unsigned(weights[i])<<" expectedWeight="<<expectedWeight<<'\n';
                    throw std::runtime_error("Prepared surface differs from independent depth, normal or eligibility reference");}
                ++checks;
            }
        }
    }
}
}
int main(int argc,char** argv)try{
    if(argc!=2)throw std::runtime_error("Expected preparation shader");test::VulkanComputeFixture host;const auto shader=readBinaryFile(argv[1]);
    fixture(host,shader,35,19,1);fixture(host,shader,35,19,2);fixture(host,shader,1,19,2);fixture(host,shader,35,1,2);fixture(host,shader,1,1,2);
    if(host.errors||host.allocator.statistics().liveImages||host.allocator.statistics().liveBuffers)throw std::runtime_error("Preparation validation or retirement failed");
    std::cout<<"PASS: "<<checks<<" full/half/odd/single-axis pixel references, cancel/retry, two epochs and slots, validation=0 resources=0\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
