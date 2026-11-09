#pragma once
#include <vulkan/vulkan.h>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace thesis {
struct Options {
    std::string path="traditional", gpu="NVIDIA", complexity="low";
    std::filesystem::path shaders, output;
    uint32_t width=1920, height=1080, frames=1000, warmup=300;
    bool validation=false;
};
struct Image { VkImage image{}; VkDeviceMemory memory{}; VkImageView view{}; VkFormat format{}; };
struct Buffer { VkBuffer buffer{}; VkDeviceMemory memory{}; VkDeviceSize size{}; };

class Benchmark {
public:
    explicit Benchmark(Options options): options(std::move(options)) {}
    ~Benchmark();
    void run();
private:
    Options options;
    VkInstance instance{};
    VkDebugUtilsMessengerEXT messenger{};
    VkPhysicalDevice physical{};
    VkPhysicalDeviceProperties properties{};
    VkDevice device{};
    VkQueue queue{};
    uint32_t queueFamily{}, timestampBits{}, vertexCount{};
    VkCommandPool commandPool{};
    VkCommandBuffer command{};
    VkFence fence{};
    VkQueryPool queries{};
    Image albedo, normal, depth, toon, output;
    Buffer vertices, readback;
    VkSampler sampler{};
    VkDescriptorPool descriptorPool{};
    VkDescriptorSetLayout sampledLayout{}, inputLayout{};
    VkPipelineLayout sampledPipelineLayout{}, inputPipelineLayout{};
    VkDescriptorSet sampledSet{}, inputSet{}, contourSet{};
    VkRenderPass geometryPass{}, toonPass{}, contourPass{};
    VkFramebuffer geometryFramebuffer{}, toonFramebuffer{}, contourFramebuffer{};
    VkPipeline geometryPipeline{}, toonPipeline{}, contourPipeline{};
    std::vector<VkShaderModule> modules;
    std::vector<double> gpuMs, cpuMs;
    std::atomic<uint32_t> errors{0}, warnings{0};
    uint64_t geometryDraws{}, toonDraws{}, contourDraws{}, nextSubpassCalls{}, beginRenderingCalls{};
    void initialize();
    uint32_t memoryType(uint32_t bits, VkMemoryPropertyFlags flags);
    void makeImage(Image& image, VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspect);
    void makeBuffer(Buffer& buffer, VkDeviceSize size, VkBufferUsageFlags usage);
    VkRenderPass makePass(const std::vector<Image*>& colors, Image* depthImage, VkImageLayout finalLayout);
    VkFramebuffer makeFramebuffer(VkRenderPass pass, const std::vector<Image*>& images);
    VkPipeline makePipeline(VkRenderPass pass, uint32_t subpass, const std::string& fragment,
                            bool geometry, uint32_t colorCount, bool dynamic=false);
    VkShaderModule shader(const std::string& name);
    void descriptors();
    void createPaths();
    void record();
    void beginPass(VkRenderPass pass, VkFramebuffer framebuffer, uint32_t colorCount, bool hasDepth);
    void geometryDraw();
    void fullscreenDraw(VkPipeline pipeline, VkPipelineLayout layout, VkDescriptorSet set);
    void submit();
    void save();
    static VKAPI_ATTR VkBool32 VKAPI_CALL debug(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
        VkDebugUtilsMessageTypeFlagsEXT, const VkDebugUtilsMessengerCallbackDataEXT* message, void* user);
};
}
