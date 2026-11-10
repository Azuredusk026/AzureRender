#include "support/VulkanComputeFixture.hpp"
#include "render/ReferenceDepth.hpp"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <limits>
using namespace azurerender;
namespace {
struct DepthInputs {
    test::VulkanComputeFixture& host;
    std::vector<rhi::GpuBuffer> buffers;
    std::vector<rhi::GpuImage> images;
    std::vector<VkImageView> views;
    std::vector<VkSampler> samplers;
    explicit DepthInputs(test::VulkanComputeFixture& h):host(h){buffers.reserve(256);images.reserve(8);views.reserve(8);samplers.reserve(8);}
    ~DepthInputs(){for(auto sampler:samplers)host.backend->destroySampler(sampler);for(auto view:views)host.backend->destroyImageView(view);for(auto image:images)host.allocator.destroyImage(image);for(auto buffer:buffers)host.allocator.destroyBuffer(buffer);}
    rhi::GpuBuffer buffer(const void* data,std::size_t bytes,VkBufferUsageFlags usage){
        auto result=host.allocator.createBuffer(bytes,usage,true);buffers.push_back(result);
        if(data)std::memcpy(result.mapped,data,bytes);else std::memset(result.mapped,0,bytes);host.allocator.flush(result,0,result.size);return result;
    }
    std::pair<VkImageView,VkSampler> texture(unsigned width,unsigned height,VkFormat format,const std::vector<std::vector<char>>& mips,const rhi::SamplerDesc& sampled){
        auto& backend=*host.backend;std::vector<char> data;std::vector<VkBufferImageCopy> copies;
        for(unsigned mip=0;mip<mips.size();++mip){while(data.size()%4)data.push_back(0);VkBufferImageCopy copy{};copy.bufferOffset=data.size();copy.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,mip,0,1};copy.imageExtent={std::max(1U,width>>mip),std::max(1U,height>>mip),1};copies.push_back(copy);data.insert(data.end(),mips[mip].begin(),mips[mip].end());}
        const auto staging=buffer(data.data(),data.size(),VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        auto image=host.allocator.createImage2D(width,height,format,VK_IMAGE_USAGE_SAMPLED_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT,static_cast<unsigned>(mips.size()));images.push_back(image);
        host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer raw){
            commands.bufferBarrier({staging.buffer,0,staging.size,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_HOST_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT});
            commands.imageBarrier({image.image,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,static_cast<unsigned>(mips.size()),VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,VK_ACCESS_TRANSFER_WRITE_BIT});
            vkCmdCopyBufferToImage(raw,staging.buffer,image.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,static_cast<unsigned>(copies.size()),copies.data());
            commands.imageBarrier({image.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,static_cast<unsigned>(mips.size()),VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT});
        });
        auto view=backend.createImageView(image.image,format,VK_IMAGE_ASPECT_COLOR_BIT,static_cast<unsigned>(mips.size()));views.push_back(view);auto sampler=backend.createSampler(sampled);samplers.push_back(sampler);return {view,sampler};
    }
    std::vector<float> render(ReferenceDepthGpu& producer,unsigned slot,const ReferenceClipProjectionProfile& profile,const std::vector<ReferenceDepthGeometry>& draws){
        const auto output=producer.output(slot);const auto bytes=std::size_t(output.extent.width)*output.extent.height*4;
        auto readback=buffer(nullptr,bytes*2,VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer raw){
            for(const auto& draw:draws){
                commands.bufferBarrier({draw.positions,0,VkDeviceSize(draw.vertexCount)*16,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,VK_ACCESS_HOST_WRITE_BIT,VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT});
                commands.bufferBarrier({draw.indices,0,VkDeviceSize(draw.indexCount)*4,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,VK_ACCESS_HOST_WRITE_BIT,VK_ACCESS_INDEX_READ_BIT});
                if(draw.surface.alpha)commands.bufferBarrier({draw.surface.alpha->uv,0,VkDeviceSize(draw.vertexCount)*8,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_VERTEX_INPUT_BIT,VK_ACCESS_HOST_WRITE_BIT,VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT});
            }
            producer.record(commands,slot,profile,draws);
            for(unsigned layer=0;layer<2;++layer){
                const auto image=layer?output.depthImage:output.image;const auto layout=layer?output.depthLayout:output.layout;
                rhi::ImageBarrierDesc barrier;barrier.image=image;barrier.aspectMask=layer?VK_IMAGE_ASPECT_DEPTH_BIT:VK_IMAGE_ASPECT_COLOR_BIT;barrier.oldLayout=layout;barrier.newLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                barrier.srcStageMask=VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;barrier.dstStageMask=VK_PIPELINE_STAGE_TRANSFER_BIT;barrier.srcAccessMask=VK_ACCESS_SHADER_READ_BIT;barrier.dstAccessMask=VK_ACCESS_TRANSFER_READ_BIT;commands.imageBarrier(barrier);
                VkBufferImageCopy copy{};copy.bufferOffset=layer*bytes;copy.imageSubresource={barrier.aspectMask,0,0,1};copy.imageExtent={output.extent.width,output.extent.height,1};vkCmdCopyImageToBuffer(raw,image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,readback.buffer,1,&copy);
                barrier.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;barrier.newLayout=layout;barrier.srcStageMask=VK_PIPELINE_STAGE_TRANSFER_BIT;barrier.dstStageMask=VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;barrier.srcAccessMask=VK_ACCESS_TRANSFER_READ_BIT;barrier.dstAccessMask=VK_ACCESS_SHADER_READ_BIT;commands.imageBarrier(barrier);
            }
            commands.bufferBarrier({readback.buffer,0,readback.size,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT});
        });
        host.allocator.invalidate(readback,0,readback.size);std::vector<float> values(bytes/2);std::memcpy(values.data(),readback.mapped,bytes*2);return values;
    }
};
}
int main(int argc,char** argv)try{
    if(argc!=2)throw std::invalid_argument("Expected depth shader directory");test::VulkanComputeFixture host(true);auto& backend=*host.backend;unsigned checked=0,rejects=0;
    for(bool framed:{false,true}){
        DepthInputs inputs(host);std::array<std::array<float,4>,4> vertices{{{-1,-1,.03125F,1},{1,-1,.03125F,1},{-1,1,.03125F,1},{1,1,.03125F,1}}};
        if(framed)for(auto& vertex:vertices){vertex[0]=.5F-vertex[0];vertex[1]-=1.F;}
        const auto near=inputs.buffer(vertices.data(),sizeof(vertices),VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);for(auto& vertex:vertices)vertex[2]=.015625F;const auto far=inputs.buffer(vertices.data(),sizeof(vertices),VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
        const std::array<std::array<float,2>,4> texcoords{{{0,0},{1,0},{0,1},{1,1}}};const auto uv=inputs.buffer(texcoords.data(),sizeof(texcoords),VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
        const std::array<unsigned,6> triangles{0,1,2,2,1,3};const auto indices=inputs.buffer(triangles.data(),sizeof(triangles),VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
        const std::array<unsigned char,4> red{0,85,170,255},blue{255,0,255,0};std::vector<std::vector<char>> mips(2);mips[0].resize(8*8*4);mips[1].resize(4*4*4);
        auto* level0=reinterpret_cast<unsigned char*>(mips[0].data());auto* level1=reinterpret_cast<unsigned char*>(mips[1].data());
        for(unsigned y=0;y<8;++y)for(unsigned x=0;x<8;++x){const auto k=(y/4)*2+x/4,at=(y*8+x)*4;level0[at]=red[k];level0[at+1]=static_cast<unsigned char>(255-red[k]);level0[at+2]=blue[k];level0[at+3]=128;}
        for(unsigned i=0;i<16;++i){level1[i*4]=255;level1[i*4+1]=0;level1[i*4+2]=128;level1[i*4+3]=128;}
        rhi::SamplerDesc sampled;sampled.filter=VK_FILTER_NEAREST;sampled.mipmapLinear=false;sampled.maxLod=1;sampled.addressU=sampled.addressV=sampled.addressW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        const auto texture=inputs.texture(8,8,VK_FORMAT_R8G8B8A8_UNORM,mips,sampled);auto shaders=ReferenceDepthShaders::loadMasked(argv[1]);ReferenceDepthConfig config;config.flipNdcY=false;
        if(framed){config.schemaVersion=2;config.positionFrame=ReferencePositionFrame{};config.positionFrame->parameters.axes={-1,2,3,0};config.positionFrame->parameters.offset={.5F,1,0,0};shaders.positionFrame=ReferenceDepthPositionFrameShaders::load(argv[1]);}
        ReferenceDepthGpu producer(backend,{8,8},2,config,shaders);
        ReferenceDepthGeometry foreground{near.buffer,indices.buffer,4,6},background{far.buffer,indices.buffer,4,6};foreground.surface.alpha=ReferenceDepthAlpha{uv.buffer,texture.first,texture.second};
        auto reject=[&](auto operation){try{operation();}catch(const std::invalid_argument&){++rejects;}};
        host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer){
            for(unsigned kind=0;kind<14;++kind){auto bad=foreground;auto& a=*bad.surface.alpha;
                if(kind==0)bad.surface.schemaVersion=3;if(kind==1)bad.surface.cullMode=VK_CULL_MODE_FRONT_AND_BACK;if(kind==2)bad.surface.frontFace=static_cast<VkFrontFace>(99);
                if(kind==3)a.uv={};if(kind==4)a.image={};if(kind==5)a.sampler={};if(kind==6)a.layout=VK_IMAGE_LAYOUT_GENERAL;if(kind==7)a.channel=4;
                if(kind==8)a.cutoff=-.1F;if(kind==9)a.cutoff=1.1F;if(kind==10)a.cutoff=std::numeric_limits<float>::quiet_NaN();if(kind==11)a.lodBias=std::numeric_limits<float>::infinity();if(kind==12)a.lodBias=17;if(kind==13)bad.surface.schemaVersion=0;
                reject([&]{producer.record(commands,0,{}, {bad});});}
            reject([&]{producer.record(commands,0,{},std::vector<ReferenceDepthGeometry>(129,foreground));});
            {auto opaqueShaders=ReferenceDepthShaders::load(argv[1]);if(framed)opaqueShaders.positionFrame=shaders.positionFrame;ReferenceDepthGpu opaque(backend,{8,8},1,config,opaqueShaders);reject([&]{opaque.record(commands,0,{}, {foreground});});}
        });
        {auto incomplete=shaders;incomplete.maskedFragment.clear();reject([&]{ReferenceDepthGpu invalid(backend,{8,8},1,config,incomplete);});}
        unsigned iteration=0;
        for(unsigned channel=0;channel<4;++channel)for(unsigned bias=0;bias<2;++bias)for(auto cull:{VK_CULL_MODE_NONE,VK_CULL_MODE_FRONT_BIT,VK_CULL_MODE_BACK_BIT})for(auto front:{VK_FRONT_FACE_CLOCKWISE,VK_FRONT_FACE_COUNTER_CLOCKWISE}){
            foreground.surface.cullMode=cull;foreground.surface.frontFace=front;auto& alpha=*foreground.surface.alpha;alpha.channel=channel;alpha.cutoff=128.F/255;alpha.lodBias=float(bias);
            const auto values=inputs.render(producer,iteration++%2,{}, {foreground,background});
            const bool frontFacing=front==VK_FRONT_FACE_CLOCKWISE,retained=cull==VK_CULL_MODE_NONE||(cull==VK_CULL_MODE_BACK_BIT?frontFacing:!frontFacing);
            for(unsigned i=0;i<128;++i){const auto p=i%64,x=p%8,y=p/8,k=(y/4)*2+x/4;const unsigned a=bias?(channel==0?255:channel==1?0:128):(channel==0?red[k]:channel==1?255-red[k]:channel==2?blue[k]:128);
                const float expected=retained&&a>=128?.03125F:.015625F;if(std::memcmp(&expected,&values[i],4))throw std::runtime_error("Depth alpha/cull/channel/mip mismatch at case "+std::to_string(iteration)+" pixel "+std::to_string(p));++checked;}
        }
        const auto empty=inputs.render(producer,0,{},{});for(float value:empty){if(value!=0)throw std::runtime_error("Masked depth clear failed");++checked;}
        for(const auto support:{MaterialUvGradientSupport{},MaterialUvGradientSupport{VK_SHADER_STAGE_COMPUTE_BIT,VK_SUBGROUP_FEATURE_QUAD_BIT},MaterialUvGradientSupport{VK_SHADER_STAGE_FRAGMENT_BIT,VK_SUBGROUP_FEATURE_BASIC_BIT}})
            reject([&]{(void)ReferenceDepthShaders::loadEllipseMasked("missing-capability-must-fail-before-file-read",support);});
        const auto support=queryMaterialUvGradientSupport(host.physical);auto ellipseShaders=ReferenceDepthShaders::loadEllipseMasked(argv[1],support);
        if(framed)ellipseShaders.positionFrame=shaders.positionFrame;
        {auto bad=ellipseShaders;bad.ellipseSupport={};reject([&]{ReferenceDepthGpu invalid(backend,{8,8},1,config,bad);});}
        {auto bad=ellipseShaders;bad.maskedVertex.clear();bad.maskedFragment.clear();reject([&]{ReferenceDepthGpu invalid(backend,{8,8},1,config,bad);});}
        host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer){
            auto bad=foreground;bad.surface.schemaVersion=2;bad.surface.alpha->lodPolicy=static_cast<ReferenceDepthAlphaLodPolicy>(99);
            reject([&]{producer.record(commands,0,{}, {bad});});
            bad.surface.schemaVersion=1;bad.surface.alpha->lodPolicy=ReferenceDepthAlphaLodPolicy::EllipseMajorAxisMirroredCoarseY;
            reject([&]{producer.record(commands,0,{}, {bad});});
            bad.surface.schemaVersion=2;reject([&]{producer.record(commands,0,{}, {bad});});
        });
        ReferenceDepthGpu ellipseProducer(backend,{8,8},2,config,ellipseShaders);
        std::vector<std::vector<char>> constantMips{std::vector<char>(8*8*4,0),std::vector<char>(4*4*4)};
        std::memset(constantMips[1].data(),255,constantMips[1].size());
        const auto constantTexture=inputs.texture(8,8,VK_FORMAT_R8G8B8A8_UNORM,constantMips,sampled);
        struct Case {std::array<float,2> dx,dy;float bias;bool level1;};
        const std::array<Case,8> cases{{{{1,0},{1,0},.125F,true},{{1,1},{1,1},-.25F,true},{{1,0},{0,1},.125F,false},
            {{-1,0},{1,0},.125F,true},{{2,0},{0,.5F},-.25F,true},{{0,0},{0,0},0,false},
            {{.0000000001F,0},{0,.0000000001F},0,false},{{1.e20F,1.e20F},{1.e20F,1.e20F},-16,true}}};
        foreground.surface={};foreground.surface.schemaVersion=2;foreground.surface.alpha=ReferenceDepthAlpha{uv.buffer,constantTexture.first,constantTexture.second};
        foreground.surface.alpha->lodPolicy=ReferenceDepthAlphaLodPolicy::EllipseMajorAxisMirroredCoarseY;
        for(const auto& testCase:cases){
            std::array<std::array<float,2>,4> coordinates{};
            coordinates[1]=testCase.dx;coordinates[2]=testCase.dy;
            for(unsigned c=0;c<2;++c)coordinates[3][c]=testCase.dx[c]+testCase.dy[c];
            const auto affine=inputs.buffer(coordinates.data(),sizeof(coordinates),VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
            foreground.surface.alpha->uv=affine.buffer;foreground.surface.alpha->lodBias=testCase.bias;
            for(unsigned channel=0;channel<4;++channel){foreground.surface.alpha->channel=channel;
                const auto values=inputs.render(ellipseProducer,iteration++%2,{}, {foreground,background});
                const float expected=testCase.level1?.03125F:.015625F;
                for(float value:values){if(std::memcmp(&expected,&value,4))throw std::runtime_error("Ellipse major-axis alpha mip mismatch at case "+std::to_string(iteration));++checked;}
            }
        }
        foreground.surface.alpha->lodPolicy=ReferenceDepthAlphaLodPolicy::Device;foreground.surface.alpha->lodBias=0;
        const auto version2Device=inputs.render(ellipseProducer,0,{}, {foreground,background});
        for(float value:version2Device){if(value!=.03125F)throw std::runtime_error("Version2 device policy compatibility failed");++checked;}
    }
    if(rejects!=50)throw std::runtime_error("Depth coverage rejection count differs");
    if(host.errors.load()||host.allocator.statistics().liveBuffers||host.allocator.statistics().liveImages)throw std::runtime_error("Depth coverage validation or resource cleanup failed");
    std::cout<<host.properties.deviceName<<": "<<checked<<" exact masked R32/D32 values; 50 rejects; relative/world frames, device/ellipse policies, versions, affine footprints, four channels, culling, slot reuse and cleanup passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
