#include "support/VulkanComputeFixture.hpp"
#include "resources/BinaryFile.hpp"
#include "render/ReferencePositionFrame.hpp"
#include "render/ReferenceDepth.hpp"
#include <algorithm>
#include <set>
#include <cstring>
#include <iostream>
#include <limits>
using namespace azurerender;
namespace {
struct Record {std::array<float,4> position,expected;};
static_assert(sizeof(Record)==32);
struct Batch {ReferencePositionFrame profile;std::vector<Record> records;};
std::vector<Batch> analytic(){
    std::vector<Batch> batches;std::array<int,3> axes{1,2,3};
    do{for(unsigned signs=0;signs<8;++signs){
        Batch batch;for(unsigned i=0;i<3;++i)batch.profile.parameters.axes[i]=(signs&(1U<<i))?-axes[i]:axes[i];
        batch.profile.parameters.offset={-.8800051212310791F,1.F,0x1p-126F,0};
        for(int i=0;i<4096;++i){Record r{};
            for(unsigned axis=0;axis<3;++axis){
                if(i%8==0){std::uint32_t word=std::uint32_t(i/8+axis)|(axis==1?0x80000000U:0U);std::memcpy(&r.position[axis],&word,4);}
                else r.position[axis]=std::ldexp(float((i+int(axis)*19)%65-32),(i%25)-12);
            }
            r.position[3]=1;r.expected[3]=1;
            for(unsigned axis=0;axis<3;++axis){const int choice=batch.profile.parameters.axes[axis];const double value=r.position[std::abs(choice)-1];r.expected[axis]=float((choice<0?-value:value)+double(batch.profile.parameters.offset[axis]));}
            batch.records.push_back(r);
        }
        batches.push_back(std::move(batch));
    }}while(std::next_permutation(axes.begin(),axes.end()));
    Batch edge;edge.profile.parameters.offset={-0.F,0.F,-0x1p-126F,0};
    edge.records={{{-0.F,-0.F,0x1.000002p-126F,1},{-0.F,0.F,0x1p-149F,1}},{{0x1p-149F,-0x1p-149F,0x1p-126F,1},{0x1p-149F,-0x1p-149F,0,1}},{{1.F,0x1p-24F,0,1},{1.F,0x1p-24F,-0x1p-126F,1}}};batches.push_back(std::move(edge));return batches;
}
}
int main(int argc,char** argv)try{
    if(argc!=2&&argc!=4)throw std::invalid_argument("Expected position frame shader, optional 32-byte frame and 32-byte input records");
    ReferencePositionFrame p;unsigned refused=0;std::set<unsigned> codes;
    const auto reject=[&](ReferencePositionFrame value){try{validateReferencePositionFrame(value);}catch(const std::invalid_argument&){++refused;}};
    p.schemaVersion=0;reject(p);p.schemaVersion=2;reject(p);p={};p.reserved[0]=1;reject(p);p={};p.parameters.axes[3]=1;reject(p);
    p={};p.parameters.offset[3]=1;reject(p);p={};p.parameters.axes[0]=0;reject(p);p.parameters.axes[0]=4;reject(p);p.parameters.axes[0]=std::numeric_limits<int>::min();reject(p);
    p={};p.parameters.axes[1]=-1;reject(p);p={};p.parameters.offset[0]=std::numeric_limits<float>::infinity();reject(p);p.parameters.offset[0]=std::numeric_limits<float>::quiet_NaN();reject(p);
    p={};try{(void)referenceRelativePosition(p,{std::numeric_limits<float>::infinity(),0,0});}catch(const std::invalid_argument&){++refused;}
    p.parameters.offset[0]=std::numeric_limits<float>::max();try{(void)referenceRelativePosition(p,{std::numeric_limits<float>::max(),0,0});}catch(const std::invalid_argument&){++refused;}
    if(refused!=13)throw std::runtime_error("Position frame rejection contract failed");
    auto batches=analytic();for(std::size_t i=0;i<48;++i)codes.insert(referencePositionFrameCode(batches[i].profile));
    if(codes.size()!=48||referencePositionFrameCode(ReferencePositionFrame{})!=136)throw std::runtime_error("Frame axis codes must be unique and canonical");
    if(argc==4){Batch external;auto profile=readBinaryFile(argv[2]),records=readBinaryFile(argv[3]);
        if(profile.size()!=32||records.empty()||records.size()%sizeof(Record)||records.size()>16*1024*1024)throw std::invalid_argument("Invalid external projection fixture sizes");
        std::memcpy(&external.profile.parameters,profile.data(),32);validateReferencePositionFrame(external.profile);
        external.records.resize(records.size()/sizeof(Record));std::memcpy(external.records.data(),records.data(),records.size());batches.push_back(std::move(external));
    }
    std::size_t points=0;
    for(const auto& batch:batches)for(const auto& record:batch.records){const auto value=referenceRelativePosition(batch.profile,{record.position[0],record.position[1],record.position[2]});
        if(std::memcmp(value.data(),record.expected.data(),12))throw std::runtime_error("CPU projection differs from independent clip oracle");++points;}
    if(std::string(argv[1])=="--cpu-only"){std::cout<<points<<" exact CPU frame points; thirteen rejects and 48 signed permutations passed\n";return 0;}
    const auto inputShader=readBinaryFile(argv[1]);
    test::VulkanComputeFixture host(true);auto& backend=*host.backend;
    VkDescriptorSetLayout setLayout{};VkDescriptorPool pool{};VkPipelineLayout layout{};VkShaderModule shader{};VkPipeline pipeline{};rhi::GpuBuffer source{},destination{};
    auto cleanup=std::unique_ptr<void,std::function<void(void*)>>(&host,[&](void*){
        if(pipeline)backend.destroyPipeline(pipeline);if(shader)backend.destroyShaderModule(shader);if(layout)backend.destroyPipelineLayout(layout);if(pool)backend.destroyDescriptorPool(pool);if(setLayout)backend.destroyDescriptorSetLayout(setLayout);
        host.allocator.destroyBuffer(source);host.allocator.destroyBuffer(destination);
    });
    setLayout=backend.createDescriptorSetLayout({{0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT},{1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT}});
    rhi::DescriptorPoolDesc descriptorPool;descriptorPool.maxSets=1;descriptorPool.sizes={{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,2}};pool=backend.createDescriptorPool(descriptorPool);const auto set=backend.allocateDescriptorSets(pool,setLayout,1).front();
    const rhi::PushConstantRangeDesc range{VK_SHADER_STAGE_COMPUTE_BIT,32};layout=backend.createPipelineLayout(setLayout,&range);shader=backend.createShaderModule(inputShader);pipeline=backend.createComputePipeline({shader,layout});
    unsigned batchIndex=0;
    for(const auto& batch:batches){
        source=host.allocator.createBuffer(batch.records.size()*sizeof(Record),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);destination=host.allocator.createBuffer(batch.records.size()*16,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
        std::memcpy(source.mapped,batch.records.data(),source.size);std::memset(destination.mapped,0xcd,destination.size);host.allocator.flush(source,0,source.size);host.allocator.flush(destination,0,destination.size);
        backend.writeDescriptorBuffer({set,0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,source.buffer,source.size});backend.writeDescriptorBuffer({set,1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,destination.buffer,destination.size});
        host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer){
            commands.bufferBarrier({source.buffer,0,source.size,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_HOST_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT});
            commands.bufferBarrier({destination.buffer,0,destination.size,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_HOST_WRITE_BIT,VK_ACCESS_SHADER_WRITE_BIT});
            commands.bindComputePipeline(pipeline);commands.bindComputeDescriptorSet(layout,set);struct Push{std::array<unsigned,4> code{};std::array<float,4> offset{};} push;push.code[0]=referencePositionFrameCode(batch.profile);push.offset=batch.profile.parameters.offset;commands.pushConstants(layout,VK_SHADER_STAGE_COMPUTE_BIT,0,&push,32);commands.dispatch((static_cast<unsigned>(batch.records.size())+63)/64,1,1);
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
    cleanup.reset();if(host.errors.load()||host.allocator.statistics().liveBuffers||host.allocator.statistics().liveImages)throw std::runtime_error("Projection Vulkan validation errors");
    std::cout<<host.properties.deviceName<<": "<<points<<" points / "<<points*4<<" exact frame components; CPU and GPU, thirteen frame rejection cases,48 signed axis permutations, cleanup passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
