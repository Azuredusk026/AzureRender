#include "support/VulkanComputeFixture.hpp"
#include "resources/BinaryFile.hpp"
#include "render/ObjectShadow.hpp"
#include <nlohmann/json.hpp>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
using namespace azurerender;
int main(int argc,char** argv) try {
    if(argc<2||argc>3)throw std::invalid_argument("Expected shader and optional depth-query fixture JSON");
    using Vec4=std::array<float,4>;
    unsigned width=16,height=8;VkFormat format=VK_FORMAT_D32_SFLOAT;
    std::vector<char> pixels(width*height*sizeof(float));
    std::vector<float> synthetic(width*height,1.F);
    synthetic[1*width+1]=.3F; // One blocker in the first 2x2 gather.
    for(unsigned y=0;y<4;++y)for(unsigned x=4;x<8;++x)synthetic[y*width+x]=.3F;
    std::memcpy(pixels.data(),synthetic.data(),pixels.size());
    std::vector<Vec4> queries={
        {.125F,.25F,.5F,0},{0,0,0,0},{0,0,.25F,.5F},
        {.375F,.25F,.5F,0},{0,0,0,0},{.25F,0,.25F,.5F},
        {.625F,.25F,.5F,0},{0,0,0,0},{.5F,0,.25F,.5F},
        {.25F,.25F,.5F,0},{0,0,0,0},{0,0,.25F,.5F}};
    std::vector<Vec4> expected={{.8625F,16,220.F/255,1},{0,64,0,1},{1,0,1,1},{1,0,1,1}};
    if(argc==3){
        std::ifstream stream(argv[2]);nlohmann::json fixture;stream>>fixture;
        width=fixture.at("width");height=fixture.at("height");format=VK_FORMAT_D16_UNORM;
        pixels=readBinaryFile(fixture.at("forwardDepthPath").get<std::string>());
        if(pixels.size()!=std::size_t(width)*height*2)throw std::runtime_error("D16 fixture dimensions mismatch");
        queries.clear();expected.clear();
        for(const auto& query:fixture.at("queries")){
            for(const auto& row:query.at("input"))queries.push_back(row.get<Vec4>());
            expected.push_back(query.at("expected").get<Vec4>());
        }
        if(queries.size()!=expected.size()*3||expected.empty())throw std::runtime_error("Invalid fixture queries");
    }
    test::VulkanComputeFixture host;auto& backend=*host.backend;
    rhi::GpuBuffer output{},records{},queryBuffer{},staging{},clearStaging{};
    rhi::GpuImage depth{},clearDepth{};VkImageView depthView{},clearView{};VkSampler sampler{};
    VkDescriptorSetLayout setLayout{};VkDescriptorPool pool{};VkPipelineLayout layout{};VkShaderModule shader{};VkPipeline pipeline{};
    auto cleanup=std::unique_ptr<void,std::function<void(void*)>>(&host,[&](void*){
        if(pipeline)backend.destroyPipeline(pipeline);if(shader)backend.destroyShaderModule(shader);
        if(layout)backend.destroyPipelineLayout(layout);if(pool)backend.destroyDescriptorPool(pool);
        if(setLayout)backend.destroyDescriptorSetLayout(setLayout);if(sampler)backend.destroySampler(sampler);
        if(depthView)backend.destroyImageView(depthView);if(clearView)backend.destroyImageView(clearView);
        host.allocator.destroyImage(depth);host.allocator.destroyImage(clearDepth);
        for(auto* buffer:{&output,&records,&queryBuffer,&staging,&clearStaging})host.allocator.destroyBuffer(*buffer);
    });
    const unsigned count=11+static_cast<unsigned>(expected.size());
    output=host.allocator.createBuffer(count*sizeof(Vec4),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
    records=host.allocator.createBuffer(32+16+2*sizeof(ObjectShadowGpuRecord),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
    queryBuffer=host.allocator.createBuffer(queries.size()*sizeof(Vec4),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
    std::memset(output.mapped,0xCD,output.size);std::memset(records.mapped,0,records.size);
    ObjectShadowGpuHeader header;header.count=2;header.capacity=2;
    auto* bytes=static_cast<char*>(records.mapped);std::memcpy(bytes+32,&header,sizeof(header));
    ObjectShadowGpuRecord r{};r.worldToUnitShadow=internal::Matrix4{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};r.biasAndFilter={.01F,.005F,0,0};r.atlasRect={0,0,.25F,.5F};
    std::memcpy(bytes+48,&r,sizeof(r));r.worldToUnitShadow[12]=3;r.biasAndFilter[0]=.02F;
    r.receiverSampling={-.5F,.5F,1,0};
    std::memcpy(bytes+48+sizeof(r),&r,sizeof(r));std::memcpy(queryBuffer.mapped,queries.data(),queryBuffer.size);
    for(auto* buffer:{&output,&records,&queryBuffer})host.allocator.flush(*buffer,0,buffer->size);
    staging=host.allocator.createBuffer(pixels.size(),VK_BUFFER_USAGE_TRANSFER_SRC_BIT,true);
    std::memcpy(staging.mapped,pixels.data(),pixels.size());host.allocator.flush(staging,0,staging.size);
    clearStaging=host.allocator.createBuffer(4*sizeof(float),VK_BUFFER_USAGE_TRANSFER_SRC_BIT,true);
    const float clearValues[]={1,1,1,1};std::memcpy(clearStaging.mapped,clearValues,sizeof(clearValues));host.allocator.flush(clearStaging,0,clearStaging.size);
    depth=host.allocator.createImage2D(width,height,format,VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT);
    clearDepth=host.allocator.createImage2D(2,2,VK_FORMAT_D32_SFLOAT,VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT);
    depthView=backend.createImageView(depth.image,format,VK_IMAGE_ASPECT_DEPTH_BIT,1);
    clearView=backend.createImageView(clearDepth.image,VK_FORMAT_D32_SFLOAT,VK_IMAGE_ASPECT_DEPTH_BIT,1);
    rhi::SamplerDesc samplerDesc;samplerDesc.filter=VK_FILTER_NEAREST;sampler=backend.createSampler(samplerDesc);
    host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer command){
        auto upload=[&](const rhi::GpuBuffer& buffer,const rhi::GpuImage& image,unsigned w,unsigned h){
            commands.bufferBarrier({buffer.buffer,0,buffer.size,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_HOST_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT});
            commands.imageBarrier({image.image,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,VK_ACCESS_TRANSFER_WRITE_BIT,VK_IMAGE_ASPECT_DEPTH_BIT});
            VkBufferImageCopy copy{};copy.imageSubresource={VK_IMAGE_ASPECT_DEPTH_BIT,0,0,1};copy.imageExtent={w,h,1};
            vkCmdCopyBufferToImage(command,buffer.buffer,image.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&copy);
            commands.imageBarrier({image.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,1,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT,VK_IMAGE_ASPECT_DEPTH_BIT});
        };upload(staging,depth,width,height);upload(clearStaging,clearDepth,2,2);
    });
    setLayout=backend.createDescriptorSetLayout({{0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT},
        {1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT},{20,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT},
        {9,VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,1,VK_SHADER_STAGE_COMPUTE_BIT},{29,VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,1,VK_SHADER_STAGE_COMPUTE_BIT},
        {28,VK_DESCRIPTOR_TYPE_SAMPLER,1,VK_SHADER_STAGE_COMPUTE_BIT}});
    rhi::DescriptorPoolDesc desc;desc.maxSets=1;desc.sizes={{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,3},{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,2},{VK_DESCRIPTOR_TYPE_SAMPLER,1}};
    pool=backend.createDescriptorPool(desc);const auto set=backend.allocateDescriptorSets(pool,setLayout,1).front();
    backend.writeDescriptorBuffer({set,0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,output.buffer,output.size});
    backend.writeDescriptorBuffer({set,1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,queryBuffer.buffer,queryBuffer.size});
    backend.writeDescriptorBuffer({set,20,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,records.buffer,records.size});
    backend.writeDescriptorImage({set,9,VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,clearView,VK_NULL_HANDLE,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
    backend.writeDescriptorImage({set,29,VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,depthView,VK_NULL_HANDLE,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
    backend.writeDescriptorImage({set,28,VK_DESCRIPTOR_TYPE_SAMPLER,VK_NULL_HANDLE,sampler,VK_IMAGE_LAYOUT_UNDEFINED});
    layout=backend.createPipelineLayout(setLayout,nullptr);shader=backend.createShaderModule(readBinaryFile(argv[1]));pipeline=backend.createComputePipeline({shader,layout});
    host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer){
        for(auto* buffer:{&records,&queryBuffer})commands.bufferBarrier({buffer->buffer,0,buffer->size,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_HOST_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT});
        commands.bufferBarrier({output.buffer,0,output.size,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_HOST_WRITE_BIT,VK_ACCESS_SHADER_WRITE_BIT});
        commands.bindComputePipeline(pipeline);commands.bindComputeDescriptorSet(layout,set);commands.dispatch(count,1,1);
        commands.bufferBarrier({output.buffer,0,output.size,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_HOST_BIT,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT});
    });
    host.allocator.invalidate(output,0,output.size);const auto* actual=static_cast<const Vec4*>(output.mapped);
    std::vector<Vec4> allExpected={{0,0,0,0},{0,.01F,.25F,1},{3,.02F,.25F,1},{0,0,0,0},{.4995F,.515F,.0184217683F,5.F/255}};
    // Reprojecting X gives NDC 2*.495/2=.495 vs .5, i.e. -0.5/200*2.
    allExpected.insert(allExpected.end(),{{-.5F,.5F,1,0},{.495F,.245F,-2,1},{.495F,.245F,-2,1},{.4975F,.2475F,-2,1},{2,.245F,-.495F,1},{.5F,.25F,-2,1}});
    allExpected.insert(allExpected.end(),expected.begin(),expected.end());
    double maximumError=0;
    for(unsigned i=0;i<count;++i)for(unsigned c=0;c<4;++c){
        const double error=std::abs(double(actual[i][c])-allExpected[i][c]);maximumError=std::max(maximumError,error);
        if(!std::isfinite(actual[i][c])||error>2e-7)throw std::runtime_error("Object shadow GPU mismatch at query "+std::to_string(i)+" channel "+std::to_string(c)+": "+std::to_string(actual[i][c])+" vs "+std::to_string(allExpected[i][c]));
    }
    cleanup.reset();if(host.errors.load())throw std::runtime_error("Object shadow Vulkan validation errors");
    std::cout<<"Object shadow GPU: "<<count<<" records/queries, maximum error="<<maximumError<<", zero validation errors\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
