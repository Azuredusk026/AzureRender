#pragma once

#include "render/GpuCulling.hpp"
#include "render/VisibilityPrototype.hpp"
#include "rhi/Rhi.hpp"
#include "rhi/IGpuAllocator.hpp"
#include <cstring>
#include <vector>

namespace azurerender {

// Frame-local storage; callers retire the frame fence before upload or release.
class GpuCullingResources final {
public:
    explicit GpuCullingResources(rhi::IRhi& backend) : backend_(backend) {}
    ~GpuCullingResources() { release(); }
    GpuCullingResources(const GpuCullingResources&) = delete;
    GpuCullingResources& operator=(const GpuCullingResources&) = delete;

    void initialize(const std::vector<char>& shaderCode, std::uint32_t capacity, bool surfaces = false) {
        if (capacity == 0 || capacity_ != 0)
            throw std::invalid_argument("GPU culling initialization requires nonzero capacity and empty resources");
        try {
            bounds_ = backend_.allocator().createBuffer(capacity * sizeof(GpuCullBounds), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, true);
            source_ = backend_.allocator().createBuffer(capacity * sizeof(VkDrawIndexedIndirectCommand), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, true);
            output_ = backend_.allocator().createBuffer(capacity * sizeof(VkDrawIndexedIndirectCommand),
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT, true);
            capacity_ = capacity;
            surfaces_ = surfaces;
            std::vector<rhi::DescriptorBindingDesc> bindings{
                {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT},
                {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT},
                {2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT}};
            if(surfaces_) {
                surfaceSource_=backend_.allocator().createBuffer(capacity*sizeof(GpuSurfaceIdentity),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
                surfaceOutput_=backend_.allocator().createBuffer(capacity*sizeof(GpuSurfaceIdentity),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
                bindings.push_back({3,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT});
                bindings.push_back({4,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT});
            }
            setLayout_ = backend_.createDescriptorSetLayout(bindings);
            rhi::DescriptorPoolDesc pool;
            pool.sizes = {{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, surfaces_?5U:3U}};
            pool.maxSets = 1;
            pool_ = backend_.createDescriptorPool(pool);
            set_ = backend_.allocateDescriptorSets(pool_, setLayout_, 1).front();
            const rhi::GpuBuffer* buffers[] = {&bounds_, &source_, &output_};
            for (std::uint32_t i = 0; i < 3; ++i)
                backend_.writeDescriptorBuffer({set_, i, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, buffers[i]->buffer, buffers[i]->size});
            if(surfaces_) {
                backend_.writeDescriptorBuffer({set_,3,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,surfaceSource_.buffer,surfaceSource_.size});
                backend_.writeDescriptorBuffer({set_,4,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,surfaceOutput_.buffer,surfaceOutput_.size});
            }
            const rhi::PushConstantRangeDesc range{VK_SHADER_STAGE_COMPUTE_BIT, sizeof(GpuCullParameters)};
            layout_ = backend_.createPipelineLayout(setLayout_, &range);
            const auto shader = backend_.createShaderModule(shaderCode);
            try { pipeline_ = backend_.createComputePipeline({shader, layout_}); }
            catch (...) { backend_.destroyShaderModule(shader); throw; }
            backend_.destroyShaderModule(shader);
            initialized_ = true;
        } catch (...) {
            release();
            throw;
        }
    }

    void upload(const std::vector<GpuCullBounds>& bounds,
                const std::vector<VkDrawIndexedIndirectCommand>& commands,
                const std::vector<GpuSurfaceIdentity>& surfaces = {}) {
        if (!initialized_) throw std::logic_error("GPU culling upload before initialization");
        if (bounds.size() > capacity_ || commands.size() > capacity_)
            throw std::out_of_range("GPU culling capacity exceeded");
        if((surfaces_ && surfaces.size()!=commands.size()) || (!surfaces_ && !surfaces.empty()))
            throw std::invalid_argument("Surface layout must match selected GPU prototype");
        for (const auto& command : commands)
            if (command.firstInstance >= bounds.size() || command.instanceCount > 1)
                throw std::invalid_argument("GPU culling command has invalid instance");
        if (!bounds.empty()) std::memcpy(bounds_.mapped, bounds.data(), bounds.size() * sizeof(GpuCullBounds));
        if (!commands.empty()) std::memcpy(source_.mapped, commands.data(), commands.size() * sizeof(VkDrawIndexedIndirectCommand));
        if(!surfaces.empty())std::memcpy(surfaceSource_.mapped,surfaces.data(),surfaces.size()*sizeof(GpuSurfaceIdentity));
        // Host-visible memory need not be coherent on every supported device.
        if(!bounds.empty())backend_.allocator().flush(bounds_,0,bounds.size()*sizeof(GpuCullBounds));
        if(!commands.empty())backend_.allocator().flush(source_,0,commands.size()*sizeof(VkDrawIndexedIndirectCommand));
        if(!surfaces.empty())backend_.allocator().flush(surfaceSource_,0,surfaces.size()*sizeof(GpuSurfaceIdentity));
        count_ = static_cast<std::uint32_t>(commands.size());
    }

    void record(rhi::ICommandRecorder& commands, GpuCullParameters parameters) const {
        if (!initialized_) throw std::logic_error("GPU culling recording before initialization");
        if (count_ == 0) return;
        commands.bindComputePipeline(pipeline_);
        commands.bindComputeDescriptorSet(layout_, set_);
        // Vulkan guarantees at least 65535 workgroups in X. Bound each
        // dispatch to that portable limit and advance stable command slots.
        for (std::uint32_t first = 0; first < count_;) {
            const auto batch = cullingDispatchBatchSize(count_ - first);
            parameters.firstCommand = first;
            parameters.drawCount = first + batch;
            commands.pushConstants(layout_, VK_SHADER_STAGE_COMPUTE_BIT, 0, &parameters, sizeof(parameters));
            commands.dispatch(batch / 64 + (batch % 64 != 0 ? 1U : 0U), 1, 1);
            first += batch;
        }
    }
    const rhi::GpuBuffer& bounds() const { return bounds_; }
    const rhi::GpuBuffer& source() const { return source_; }
    const rhi::GpuBuffer& output() const { return output_; }
    const rhi::GpuBuffer& surfaceSource() const { return surfaceSource_; }
    const rhi::GpuBuffer& surfaceOutput() const { return surfaceOutput_; }
private:
    void release() noexcept {
        if (pipeline_) backend_.destroyPipeline(pipeline_);
        if (layout_) backend_.destroyPipelineLayout(layout_);
        if (pool_) backend_.destroyDescriptorPool(pool_);
        if (setLayout_) backend_.destroyDescriptorSetLayout(setLayout_);
        backend_.allocator().destroyBuffer(output_);
        backend_.allocator().destroyBuffer(source_);
        backend_.allocator().destroyBuffer(bounds_);
        backend_.allocator().destroyBuffer(surfaceOutput_);
        backend_.allocator().destroyBuffer(surfaceSource_);
        pipeline_ = VK_NULL_HANDLE;
        layout_ = VK_NULL_HANDLE;
        pool_ = VK_NULL_HANDLE;
        setLayout_ = VK_NULL_HANDLE;
        set_ = VK_NULL_HANDLE;
        capacity_ = 0;
        count_ = 0;
        initialized_ = false;
        surfaces_ = false;
    }
    rhi::IRhi& backend_;
    rhi::GpuBuffer bounds_{}, source_{}, output_{};
    rhi::GpuBuffer surfaceSource_{},surfaceOutput_{};
    VkDescriptorSetLayout setLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool pool_ = VK_NULL_HANDLE;
    VkDescriptorSet set_ = VK_NULL_HANDLE;
    VkPipelineLayout layout_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
    std::uint32_t capacity_ = 0, count_ = 0;
    bool initialized_ = false;
    bool surfaces_ = false;
};

}  // namespace azurerender
