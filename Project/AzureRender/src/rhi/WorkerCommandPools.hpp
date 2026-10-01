#pragma once

#include <vulkan/vulkan.h>
#include <stdexcept>
#include <vector>

namespace azurerender::rhi {

// One pool per worker per in-flight frame. The caller joins CPU workers and
// retires GPU work before resetFrame or destruction.
class WorkerCommandPools final {
public:
    WorkerCommandPools(VkDevice device, std::uint32_t family,
                       std::size_t frames, std::size_t workers)
        : device_(device), workers_(workers), pools_(frames * workers),
          buffers_(frames * workers), cursors_(frames * workers, 0) {
        if (device == VK_NULL_HANDLE || frames == 0 || workers == 0)
            throw std::invalid_argument("Worker command pools require a device and nonzero dimensions");
        VkCommandPoolCreateInfo info{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        info.queueFamilyIndex = family;
        info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        try {
            for (auto& pool : pools_)
                check(vkCreateCommandPool(device_, &info, nullptr, &pool));
        } catch (...) {
            destroy();
            throw;
        }
    }
    ~WorkerCommandPools() { destroy(); }
    WorkerCommandPools(const WorkerCommandPools&) = delete;
    WorkerCommandPools& operator=(const WorkerCommandPools&) = delete;

    void resetFrame(std::size_t frame, VkFence completion) {
        if (frame >= pools_.size() / workers_)
            throw std::out_of_range("Worker command pool frame index");
        if (completion == VK_NULL_HANDLE
            || vkGetFenceStatus(device_, completion) != VK_SUCCESS)
            throw std::logic_error("Worker pools cannot reset before frame completion");
        for (std::size_t worker = 0; worker < workers_; ++worker) {
            check(vkResetCommandPool(device_, pools_[frame * workers_ + worker], 0));
            cursors_[frame * workers_ + worker] = 0;
        }
    }

    VkCommandBuffer allocate(std::size_t frame, std::size_t worker) {
        if (worker >= workers_ || frame >= pools_.size() / workers_)
            throw std::out_of_range("Worker command pool index");
        const auto slot = frame * workers_ + worker;
        auto& cursor = cursors_[slot];
        auto& buffers = buffers_[slot];
        if (cursor < buffers.size()) return buffers[cursor++];
        VkCommandBufferAllocateInfo info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        info.commandPool = pools_[frame * workers_ + worker];
        info.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
        info.commandBufferCount = 1;
        VkCommandBuffer buffer = VK_NULL_HANDLE;
        check(vkAllocateCommandBuffers(device_, &info, &buffer));
        buffers.push_back(buffer);
        ++cursor;
        return buffer;
    }
private:
    static void check(VkResult result) {
        if (result != VK_SUCCESS) throw std::runtime_error("Worker command pool Vulkan operation failed");
    }
    void destroy() noexcept {
        for (const auto pool : pools_)
            if (pool != VK_NULL_HANDLE) vkDestroyCommandPool(device_, pool, nullptr);
    }
    VkDevice device_;
    std::size_t workers_;
    std::vector<VkCommandPool> pools_;
    std::vector<std::vector<VkCommandBuffer>> buffers_;
    std::vector<std::size_t> cursors_;
};

}  // namespace azurerender::rhi
