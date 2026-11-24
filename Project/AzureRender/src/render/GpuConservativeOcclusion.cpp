#include "GpuConservativeOcclusion.hpp"
#include "rhi/IGpuAllocator.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
namespace azurerender {
struct GpuConservativeOcclusion::Impl {
    struct Frame {rhi::GpuBuffer bounds{},settings{},output{},counter{};VkDescriptorSet set{};std::uint64_t generation=0;std::uint32_t count=0;bool valid=false,recorded=false;};
    rhi::IRhi& rhi;std::uint32_t capacity=0;std::uint64_t bytes=0;std::vector<Frame> frames;
    VkDescriptorSetLayout descriptors{};VkDescriptorPool pool{};VkPipelineLayout layout{};VkPipeline pipeline{};
    explicit Impl(rhi::IRhi& backend):rhi(backend){}
    ~Impl(){
        if(pipeline)rhi.destroyPipeline(pipeline);if(layout)rhi.destroyPipelineLayout(layout);
        if(pool)rhi.destroyDescriptorPool(pool);if(descriptors)rhi.destroyDescriptorSetLayout(descriptors);
        for(auto& f:frames)for(auto* b:{&f.bounds,&f.settings,&f.output,&f.counter})rhi.allocator().destroyBuffer(*b);
    }
    void initialize(std::uint32_t count,unsigned slots,const std::vector<char>& shader){
        const auto size=std::uint64_t(slots)*(std::uint64_t(count)*116+52);
        if(!count||count>1048576||!slots||slots>8||size>kMaximumBytes)throw std::invalid_argument("Invalid conservative occlusion capacity or pool budget");
        std::uint32_t magic=0;if(shader.size()>=4)std::memcpy(&magic,shader.data(),4);
        if(shader.size()<20||shader.size()>1024*1024||shader.size()%4||magic!=0x07230203U)throw std::invalid_argument("Occlusion requires bounded SPIR-V");
        capacity=count;bytes=size;
        descriptors=rhi.createDescriptorSetLayout({{0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT},
            {1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT},{2,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT},
            {3,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,1,VK_SHADER_STAGE_COMPUTE_BIT},{4,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT},
            {5,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT}});
        layout=rhi.createPipelineLayout(descriptors,nullptr);auto module=rhi.createShaderModule(shader);
        try{pipeline=rhi.createComputePipeline({module,layout});}catch(...){rhi.destroyShaderModule(module);throw;}
        rhi.destroyShaderModule(module);rhi::DescriptorPoolDesc description;description.maxSets=slots;
        description.sizes={{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,slots*4},{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,slots},{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,slots}};
        pool=rhi.createDescriptorPool(description);const auto sets=rhi.allocateDescriptorSets(pool,descriptors,slots);frames.resize(slots);
        for(unsigned slot=0;slot<slots;++slot){auto& f=frames[slot];f.set=sets.at(slot);
            f.bounds=rhi.allocator().createBuffer(VkDeviceSize(count)*96,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
            f.settings=rhi.allocator().createBuffer(48,VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,true);
            f.output=rhi.allocator().createBuffer(VkDeviceSize(count)*20,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_SRC_BIT,false);
            f.counter=rhi.allocator().createBuffer(4,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
            if(!f.bounds.mapped||!f.settings.mapped||!f.counter.mapped)throw std::runtime_error("Occlusion frame storage is not mapped");
            rhi.writeDescriptorBuffer({f.set,0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,f.bounds.buffer,f.bounds.size});
            rhi.writeDescriptorBuffer({f.set,2,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,f.output.buffer,f.output.size});
            rhi.writeDescriptorBuffer({f.set,4,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,f.settings.buffer,f.settings.size});
            rhi.writeDescriptorBuffer({f.set,5,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,f.counter.buffer,f.counter.size});
        }
    }
};
GpuConservativeOcclusion::GpuConservativeOcclusion(rhi::IRhi& backend,std::uint32_t capacity,unsigned slots,const std::vector<char>& shader)
    :impl_(std::make_unique<Impl>(backend)){impl_->initialize(capacity,slots,shader);}
GpuConservativeOcclusion::~GpuConservativeOcclusion()=default;
GpuConservativeOcclusion::Ticket GpuConservativeOcclusion::prepare(unsigned slot,const rhi::GpuBuffer& source,
    const std::vector<GpuOcclusionBounds>& bounds,const DepthPyramidOutput& depth,VkViewport viewport,float bias,unsigned guard){
    auto& p=*impl_;auto& f=p.frames.at(slot);f.valid=false;
    if(bounds.size()>p.capacity)throw std::length_error("Occlusion draw capacity exceeded");
    unsigned full=1;for(auto side=std::max(depth.extent.width,depth.extent.height);side>1;side/=2)++full;
    if(!depth.image||!depth.view||!depth.sampler||!depth.extent.width||!depth.extent.height||depth.extent.width>8192||depth.extent.height>8192
        ||!depth.levels||depth.levels>full)throw std::invalid_argument("Occlusion requires a bounded full-size depth pyramid");
    for(float v:{viewport.x,viewport.y,viewport.width,viewport.height,viewport.minDepth,viewport.maxDepth,bias})
        if(!std::isfinite(v))throw std::invalid_argument("Nonfinite occlusion viewport or bias");
    if(!viewport.width||!viewport.height||std::abs(viewport.width)>8192||std::abs(viewport.height)>8192
        ||std::abs(viewport.x)>16384||std::abs(viewport.y)>16384||viewport.minDepth!=0||viewport.maxDepth!=1||bias<0||bias>1||guard>8)
        throw std::invalid_argument("Occlusion requires a forward viewport and bounded safety margins");
    for(const auto& b:bounds){
        for(float v:b.clipFromLocal)if(!std::isfinite(v))throw std::invalid_argument("Nonfinite occlusion matrix");
        for(unsigned a=0;a<3;++a)if(!std::isfinite(b.minimum[a])||!std::isfinite(b.maximum[a])||b.minimum[a]>b.maximum[a])
            throw std::invalid_argument("Invalid occlusion bounds");
    }
    if(!bounds.empty()){
        if(!source.buffer||source.size<bounds.size()*20)throw std::invalid_argument("Occlusion source command range is too small");
        for(const auto& owned:p.frames)for(const auto* b:{&owned.bounds,&owned.settings,&owned.output,&owned.counter})
            if(source.buffer==b->buffer)throw std::invalid_argument("Occlusion source aliases owned storage");
    }
    if(f.generation==UINT64_MAX)throw std::overflow_error("Occlusion frame generation exhausted");
    struct alignas(16) Settings {std::array<float,4> viewport;std::array<std::uint32_t,4> dimensions;std::array<float,4> safety;};
    static_assert(sizeof(Settings)==48,"Occlusion settings must match std140");
    const Settings settings{{viewport.x,viewport.y,viewport.width,viewport.height},
        {static_cast<std::uint32_t>(bounds.size()),depth.extent.width,depth.extent.height,depth.levels},{bias,static_cast<float>(guard),0,0}};
    std::memcpy(f.settings.mapped,&settings,sizeof(settings));p.rhi.allocator().flush(f.settings,0,48);
    const std::uint32_t zero=0;std::memcpy(f.counter.mapped,&zero,4);p.rhi.allocator().flush(f.counter,0,4);
    if(!bounds.empty()){
        std::memcpy(f.bounds.mapped,bounds.data(),bounds.size()*96);p.rhi.allocator().flush(f.bounds,0,bounds.size()*96);
        p.rhi.writeDescriptorBuffer({f.set,1,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,source.buffer,source.size});
        p.rhi.writeDescriptorImage({f.set,3,VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,depth.view,depth.sampler,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
    }
    f.count=static_cast<std::uint32_t>(bounds.size());f.valid=true;f.recorded=false;++f.generation;
    return {slot,f.generation,f.count};
}
void GpuConservativeOcclusion::record(rhi::ICommandRecorder& commands,Ticket ticket){
    auto& p=*impl_;auto& f=p.frames.at(ticket.slot);
    if(!f.valid||f.recorded||f.generation!=ticket.generation||f.count!=ticket.count)throw std::logic_error("Occlusion ticket is stale or consumed");
    f.recorded=true;if(!ticket.count)return;
    commands.bufferBarrier({f.bounds.buffer,0,VkDeviceSize(ticket.count)*96,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_HOST_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT});
    commands.bufferBarrier({f.settings.buffer,0,48,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_HOST_WRITE_BIT,VK_ACCESS_UNIFORM_READ_BIT});
    commands.bufferBarrier({f.counter.buffer,0,4,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_HOST_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT});
    commands.bindComputePipeline(p.pipeline);commands.bindComputeDescriptorSet(p.layout,f.set);commands.dispatch((ticket.count+63)/64,1,1);
    commands.bufferBarrier({f.output.buffer,0,VkDeviceSize(ticket.count)*20,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT});
    commands.bufferBarrier({f.counter.buffer,0,4,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_HOST_BIT,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT});
}
const rhi::GpuBuffer& GpuConservativeOcclusion::output(unsigned slot)const{return impl_->frames.at(slot).output;}
std::uint32_t GpuConservativeOcclusion::retiredZeroCommandCount(unsigned slot)const{
    auto& p=*impl_;const auto& f=p.frames.at(slot);if(!f.valid||!f.recorded||!f.count)return 0;
    p.rhi.allocator().invalidate(f.counter,0,4);std::uint32_t zero=0;std::memcpy(&zero,f.counter.mapped,4);return zero;
}
std::uint64_t GpuConservativeOcclusion::bytes()const noexcept{return impl_->bytes;}
}
