#include "support/VulkanComputeFixture.hpp"
#include "resources/BinaryFile.hpp"
#include "render/ReferenceClipProjection.hpp"
#include "render/ReferenceFmaShader.hpp"
#include <cstring>
#include <iostream>
#include <limits>
using namespace azurerender;
namespace {
struct Record {std::array<float,4> position,expected;};
static_assert(sizeof(Record)==32);
struct Batch {ReferenceClipProjectionProfile profile;std::vector<Record> records;};
Batch analytic(bool perspective){
    Batch batch;auto& p=batch.profile.parameters;
    if(perspective){p.columns={ReferenceClipProjectionParameters::Row{2,0,0,0},{0,-4,0,0},{.125F,.25F,.5F,-1},{.5F,-.25F,1,0}};p.jitter={1.F/256,-1.F/512,0,0};}
    for(int i=0;i<4096;++i){
        const std::array<float,3> point{float(i%31-15)/8,float(i%47-23)/16,-float(i%53+1)/8};Record r{{point[0],point[1],point[2],1},{}};
        for(unsigned row=0;row<4;++row){double value=p.columns[3][row];for(unsigned axis=0;axis<3;++axis)value+=double(point[axis])*p.columns[axis][row];r.expected[row]=float(value);}
        r.expected[0]=float(double(r.expected[0])-2*double(r.expected[3])*p.jitter[0]);
        r.expected[1]=float(double(r.expected[1])+2*double(r.expected[3])*p.jitter[1]);batch.records.push_back(r);
    }
    return batch;
}
}
int main(int argc,char** argv)try{
    if(argc!=2&&argc!=4)throw std::invalid_argument("Expected projection shader, optional 80-byte profile and 32-byte input records");
    ReferenceClipProjectionProfile p;unsigned refused=0;
    const auto reject=[&](ReferenceClipProjectionProfile value){try{validateReferenceClipProjection(value);}catch(const std::invalid_argument&){++refused;}};
    p.reserved[1]=1;reject(p);p={};p.schemaVersion=0;reject(p);p.schemaVersion=2;reject(p);p={};p.parameters.jitter[2]=1;reject(p);
    p={};p.parameters.jitter[0]=1.01F;reject(p);p={};p.parameters.columns[0][0]=std::numeric_limits<float>::infinity();reject(p);
    p={};p.parameters.jitter[1]=std::numeric_limits<float>::quiet_NaN();reject(p);p={};
    try{(void)projectReferenceClip(p,{std::numeric_limits<float>::infinity(),0,0});}catch(const std::invalid_argument&){++refused;}
    p.parameters.columns[0][0]=std::numeric_limits<float>::max();try{(void)projectReferenceClip(p,{4,0,0});}catch(const std::invalid_argument&){++refused;}
    if(refused!=9)throw std::runtime_error("Projection rejection contract failed");
    std::vector<Batch> batches{analytic(false),analytic(true)};
    if(argc==4){Batch external;auto profile=readBinaryFile(argv[2]),records=readBinaryFile(argv[3]);
        if(profile.size()!=80||records.empty()||records.size()%sizeof(Record)||records.size()>16*1024*1024)throw std::invalid_argument("Invalid external projection fixture sizes");
        std::memcpy(&external.profile.parameters,profile.data(),80);validateReferenceClipProjection(external.profile);
        external.records.resize(records.size()/sizeof(Record));std::memcpy(external.records.data(),records.data(),records.size());batches.push_back(std::move(external));
    }
    std::size_t points=0;
    for(const auto& batch:batches)for(const auto& record:batch.records){const auto value=projectReferenceClip(batch.profile,{record.position[0],record.position[1],record.position[2]});
        if(std::memcmp(value.data(),record.expected.data(),16))throw std::runtime_error("CPU projection differs from independent clip oracle");++points;}
    const auto inputShader=readBinaryFile(argv[1]);const auto prepared=prepareReferenceFmaShader(inputShader);
    const auto repeated=prepareReferenceFmaShader(prepared.code);
    if(prepared.fusedResults!=3||repeated.code!=prepared.code||repeated.fusedResults!=3)throw std::runtime_error("Reference Fma annotation preparation must be complete and idempotent");
    unsigned shaderRejects=0;
    for(unsigned kind=0;kind<5;++kind){auto broken=inputShader;
        if(kind==0)broken.resize(16);
        if(kind==1)broken[0]=0;
        if(kind==2)broken.pop_back();
        if(kind==3){std::uint32_t zero=0;std::memcpy(broken.data()+20,&zero,4);}
        if(kind==4){for(std::size_t at=20;at<broken.size();){std::uint32_t word;std::memcpy(&word,broken.data()+at,4);const auto count=word>>16;
            if((word&0xffffU)==71U&&count==3U){std::uint32_t decoration;std::memcpy(&decoration,broken.data()+at+8,4);if(decoration==42){const std::uint32_t relaxed=0;std::memcpy(broken.data()+at+8,&relaxed,4);}}
            at+=std::size_t(count)*4;}}
        try{(void)prepareReferenceFmaShader(broken);}catch(const std::invalid_argument&){++shaderRejects;}
    }
    if(shaderRejects!=5)throw std::runtime_error("Reference shader rejection contract failed");
    test::VulkanComputeFixture host(true);auto& backend=*host.backend;
    VkDescriptorSetLayout setLayout{};VkDescriptorPool pool{};VkPipelineLayout layout{};VkShaderModule shader{};VkPipeline pipeline{};rhi::GpuBuffer source{},destination{};
    auto cleanup=std::unique_ptr<void,std::function<void(void*)>>(&host,[&](void*){
        if(pipeline)backend.destroyPipeline(pipeline);if(shader)backend.destroyShaderModule(shader);if(layout)backend.destroyPipelineLayout(layout);if(pool)backend.destroyDescriptorPool(pool);if(setLayout)backend.destroyDescriptorSetLayout(setLayout);
        host.allocator.destroyBuffer(source);host.allocator.destroyBuffer(destination);
    });
    setLayout=backend.createDescriptorSetLayout({{0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT},{1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT}});
    rhi::DescriptorPoolDesc descriptorPool;descriptorPool.maxSets=1;descriptorPool.sizes={{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,2}};pool=backend.createDescriptorPool(descriptorPool);const auto set=backend.allocateDescriptorSets(pool,setLayout,1).front();
    const rhi::PushConstantRangeDesc range{VK_SHADER_STAGE_COMPUTE_BIT,80};layout=backend.createPipelineLayout(setLayout,&range);shader=backend.createShaderModule(prepared.code);pipeline=backend.createComputePipeline({shader,layout});
    unsigned batchIndex=0;
    for(const auto& batch:batches){
        source=host.allocator.createBuffer(batch.records.size()*sizeof(Record),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);destination=host.allocator.createBuffer(batch.records.size()*16,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
        std::memcpy(source.mapped,batch.records.data(),source.size);std::memset(destination.mapped,0xcd,destination.size);host.allocator.flush(source,0,source.size);host.allocator.flush(destination,0,destination.size);
        backend.writeDescriptorBuffer({set,0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,source.buffer,source.size});backend.writeDescriptorBuffer({set,1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,destination.buffer,destination.size});
        host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer){
            commands.bufferBarrier({source.buffer,0,source.size,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_HOST_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT});
            commands.bufferBarrier({destination.buffer,0,destination.size,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_HOST_WRITE_BIT,VK_ACCESS_SHADER_WRITE_BIT});
            commands.bindComputePipeline(pipeline);commands.bindComputeDescriptorSet(layout,set);commands.pushConstants(layout,VK_SHADER_STAGE_COMPUTE_BIT,0,&batch.profile.parameters,80);commands.dispatch((static_cast<unsigned>(batch.records.size())+63)/64,1,1);
            commands.bufferBarrier({destination.buffer,0,destination.size,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_HOST_BIT,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT});
        });
        host.allocator.invalidate(destination,0,destination.size);const auto results=static_cast<const std::array<float,4>*>(destination.mapped);
        for(std::size_t i=0;i<batch.records.size();++i)if(std::memcmp(results[i].data(),batch.records[i].expected.data(),16)){
            std::cerr<<"batch="<<batchIndex<<" point="<<i<<" position=";
            for(float v:batch.records[i].position)std::cerr<<v<<",";
            std::cerr<<" expectedBits=";
            for(float v:batch.records[i].expected){std::uint32_t word;std::memcpy(&word,&v,4);std::cerr<<std::hex<<word<<",";}
            std::cerr<<" actualBits=";
            for(float v:results[i]){std::uint32_t word;std::memcpy(&word,&v,4);std::cerr<<std::hex<<word<<",";}
            std::cerr<<std::dec<<"\\n";
            throw std::runtime_error("GPU projection differs from independent clip oracle");}
        ++batchIndex;
        host.allocator.destroyBuffer(source);source={};host.allocator.destroyBuffer(destination);destination={};
    }
    cleanup.reset();if(host.errors.load())throw std::runtime_error("Projection Vulkan validation errors");
    std::cout<<host.properties.deviceName<<": "<<points<<" points / "<<points*4<<" exact clip components; CPU and GPU, nine projection and five shader rejection cases, cleanup passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
