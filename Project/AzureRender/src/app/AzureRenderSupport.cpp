#include "AzureRenderApp.hpp"
#include "platform/GlfwFrontend.hpp"
#include "AzureRenderInternal.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

using namespace azurerender::internal;

namespace {

constexpr std::array<const char*, 1> kValidationLayers = {
    "VK_LAYER_KHRONOS_validation",
};

constexpr std::array<const char*, 1> kDeviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};

}  // namespace

AzureRenderApp::QueueFamilyIndices AzureRenderApp::findQueueFamilies(
    const VkPhysicalDevice device) const {
    QueueFamilyIndices indices;
    std::uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    for (std::uint32_t index = 0; index < queueFamilyCount; ++index) {
        if ((queueFamilies[index].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0U) {
            indices.graphics = index;
        }
        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, index, surface_, &presentSupport);
        if (presentSupport == VK_TRUE) {
            indices.present = index;
        }
        if (indices.complete()) {
            break;
        }
    }
    return indices;
}

AzureRenderApp::SwapchainSupport AzureRenderApp::querySwapchainSupport(
    const VkPhysicalDevice device) const {
    SwapchainSupport support;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface_, &support.capabilities);

    std::uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &formatCount, nullptr);
    support.formats.resize(formatCount);
    if (formatCount > 0) {
        vkGetPhysicalDeviceSurfaceFormatsKHR(
            device, surface_, &formatCount, support.formats.data());
    }

    std::uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &presentModeCount, nullptr);
    support.presentModes.resize(presentModeCount);
    if (presentModeCount > 0) {
        vkGetPhysicalDeviceSurfacePresentModesKHR(
            device, surface_, &presentModeCount, support.presentModes.data());
    }
    return support;
}

bool AzureRenderApp::isDeviceSuitable(const VkPhysicalDevice device) const {
    const QueueFamilyIndices indices = findQueueFamilies(device);
    const bool extensionsSupported = checkDeviceExtensionSupport(device);
    bool swapchainAdequate = false;
    if (extensionsSupported) {
        const SwapchainSupport support = querySwapchainSupport(device);
        swapchainAdequate = !support.formats.empty() && !support.presentModes.empty();
    }
    return indices.complete() && extensionsSupported && swapchainAdequate;
}

bool AzureRenderApp::checkDeviceExtensionSupport(const VkPhysicalDevice device) const {
    std::uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> available(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, available.data());
    std::set<std::string> required(kDeviceExtensions.begin(), kDeviceExtensions.end());
    for (const auto& extension : available) {
        required.erase(extension.extensionName);
    }
    return required.empty();
}

bool AzureRenderApp::checkValidationLayerSupport() const {
    std::uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> available(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, available.data());

    return std::all_of(
        kValidationLayers.begin(),
        kValidationLayers.end(),
        [&available](const char* requiredLayer) {
            return std::any_of(
                available.begin(),
                available.end(),
                [requiredLayer](const VkLayerProperties& layer) {
                    return std::strcmp(requiredLayer, layer.layerName) == 0;
                });
        });
}

void AzureRenderApp::recreateSwapchain() {
    int width = 0;
    int height = 0;
    while (width == 0 || height == 0) {
        const auto size = frontend_->framebufferSize();
        width = size.first;
        height = size.second;
        frontend_->waitEvents();
    }

    vkDeviceWaitIdle(device_);
    cleanupSwapchain();
    createSwapchain();
    createImageViews();
    createSceneColorResources();
    createDepthResources();
    createNormalResources();
    createRenderPass();
    createPostProcessRenderPass();
    createGraphicsPipeline();
    createFramebuffers();
    createPostProcessFramebuffers();
    createPostProcessDescriptorSets();
    createSwapchainSemaphores();
}

void AzureRenderApp::cleanupSwapchain() {
    if (postProcessDescriptorPool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(
            device_,
            postProcessDescriptorPool_,
            nullptr);
        postProcessDescriptorPool_ = VK_NULL_HANDLE;
    }
    postProcessDescriptorSets_.clear();

    for (const auto semaphore : renderFinishedSemaphores_) {
        vkDestroySemaphore(device_, semaphore, nullptr);
    }
    renderFinishedSemaphores_.clear();

    for (const auto framebuffer : swapchainFramebuffers_) {
        vkDestroyFramebuffer(device_, framebuffer, nullptr);
    }
    swapchainFramebuffers_.clear();
    for (const auto framebuffer : postProcessFramebuffers_) {
        vkDestroyFramebuffer(device_, framebuffer, nullptr);
    }
    postProcessFramebuffers_.clear();

    for (const auto imageView : sceneColorImageViews_) {
        vkDestroyImageView(device_, imageView, nullptr);
    }
    for (const auto image : sceneColorImages_) {
        vkDestroyImage(device_, image, nullptr);
    }
    for (const auto memory : sceneColorImageMemories_) {
        vkFreeMemory(device_, memory, nullptr);
    }
    sceneColorImageViews_.clear();
    sceneColorImages_.clear();
    sceneColorImageMemories_.clear();

    for (const auto imageView : depthImageViews_) {
        vkDestroyImageView(device_, imageView, nullptr);
    }
    for (const auto image : depthImages_) {
        vkDestroyImage(device_, image, nullptr);
    }
    for (const auto memory : depthImageMemories_) {
        vkFreeMemory(device_, memory, nullptr);
    }
    depthImageViews_.clear();
    depthImages_.clear();
    depthImageMemories_.clear();

    for (const auto imageView : normalImageViews_) {
        vkDestroyImageView(device_, imageView, nullptr);
    }
    for (const auto image : normalImages_) {
        vkDestroyImage(device_, image, nullptr);
    }
    for (const auto memory : normalImageMemories_) {
        vkFreeMemory(device_, memory, nullptr);
    }
    normalImageViews_.clear();
    normalImages_.clear();
    normalImageMemories_.clear();

    for (VkPipeline* pipeline : {
             &opaquePipeline_,
             &opaqueDoubleSidedPipeline_,
             &blendPipeline_,
             &blendDoubleSidedPipeline_,
             &outlinePipeline_,
             &backgroundPipeline_,
             &shadowPipeline_}) {
        if (*pipeline != VK_NULL_HANDLE) {
            vkDestroyPipeline(device_, *pipeline, nullptr);
            *pipeline = VK_NULL_HANDLE;
        }
    }
    if (innerOutlinePipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, innerOutlinePipeline_, nullptr);
        innerOutlinePipeline_ = VK_NULL_HANDLE;
    }
    if (hudPipeline_ != VK_NULL_HANDLE) {
        vkDestroyPipeline(device_, hudPipeline_, nullptr);
        hudPipeline_ = VK_NULL_HANDLE;
    }
    if (pipelineLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr);
        pipelineLayout_ = VK_NULL_HANDLE;
    }
    if (postProcessPipelineLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(
            device_,
            postProcessPipelineLayout_,
            nullptr);
        postProcessPipelineLayout_ = VK_NULL_HANDLE;
    }
    if (hudPipelineLayout_ != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(
            device_,
            hudPipelineLayout_,
            nullptr);
        hudPipelineLayout_ = VK_NULL_HANDLE;
    }
    if (renderPass_ != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device_, renderPass_, nullptr);
        renderPass_ = VK_NULL_HANDLE;
    }
    if (postProcessRenderPass_ != VK_NULL_HANDLE) {
        vkDestroyRenderPass(
            device_,
            postProcessRenderPass_,
            nullptr);
        postProcessRenderPass_ = VK_NULL_HANDLE;
    }
    for (const auto imageView : swapchainImageViews_) {
        vkDestroyImageView(device_, imageView, nullptr);
    }
    swapchainImageViews_.clear();
    swapchainImages_.clear();

    if (swapchain_ != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(device_, swapchain_, nullptr);
        swapchain_ = VK_NULL_HANDLE;
    }
}

std::vector<const char*> AzureRenderApp::requiredInstanceExtensions() const {
    std::vector<const char*> extensions =
        frontend_->requiredVulkanExtensions();
    if (kEnableValidation) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    return extensions;
}

VkSurfaceFormatKHR AzureRenderApp::chooseSurfaceFormat(
    const std::vector<VkSurfaceFormatKHR>& formats) {
    const auto preferred = std::find_if(
        formats.begin(),
        formats.end(),
        [](const VkSurfaceFormatKHR& format) {
            return format.format == VK_FORMAT_B8G8R8A8_SRGB
                && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        });
    return preferred != formats.end() ? *preferred : formats.front();
}

VkPresentModeKHR AzureRenderApp::choosePresentMode(
    const std::vector<VkPresentModeKHR>& presentModes) {
    return std::find(presentModes.begin(), presentModes.end(), VK_PRESENT_MODE_MAILBOX_KHR)
            != presentModes.end()
        ? VK_PRESENT_MODE_MAILBOX_KHR
        : VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D AzureRenderApp::chooseExtent(
    const VkSurfaceCapabilitiesKHR& capabilities) const {
    if (capabilities.currentExtent.width != std::numeric_limits<std::uint32_t>::max()) {
        return capabilities.currentExtent;
    }

    const auto [width, height] = frontend_->framebufferSize();
    VkExtent2D extent{
        static_cast<std::uint32_t>(width),
        static_cast<std::uint32_t>(height),
    };
    extent.width = std::clamp(
        extent.width,
        capabilities.minImageExtent.width,
        capabilities.maxImageExtent.width);
    extent.height = std::clamp(
        extent.height,
        capabilities.minImageExtent.height,
        capabilities.maxImageExtent.height);
    return extent;
}

std::uint32_t AzureRenderApp::findMemoryType(
    const std::uint32_t typeFilter,
    const VkMemoryPropertyFlags properties) const {
    VkPhysicalDeviceMemoryProperties memoryProperties{};
    vkGetPhysicalDeviceMemoryProperties(physicalDevice_, &memoryProperties);
    for (std::uint32_t index = 0; index < memoryProperties.memoryTypeCount; ++index) {
        if ((typeFilter & (1U << index)) != 0U
            && (memoryProperties.memoryTypes[index].propertyFlags & properties) == properties) {
            return index;
        }
    }
    throw std::runtime_error("No suitable Vulkan memory type was found");
}

VkFormat AzureRenderApp::findDepthFormat() const {
    constexpr std::array candidates = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT,
    };
    for (const VkFormat format : candidates) {
        VkFormatProperties properties{};
        vkGetPhysicalDeviceFormatProperties(physicalDevice_, format, &properties);
        constexpr VkFormatFeatureFlags requiredFeatures =
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
            | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
        if ((properties.optimalTilingFeatures & requiredFeatures)
            == requiredFeatures) {
            return format;
        }
    }
    throw std::runtime_error("No supported depth buffer format was found");
}

void AzureRenderApp::createBuffer(
    const VkDeviceSize size,
    const VkBufferUsageFlags usage,
    const VkMemoryPropertyFlags properties,
    VkBuffer& buffer,
    VkDeviceMemory& memory) const {
    VkBufferCreateInfo bufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vkCheck(vkCreateBuffer(device_, &bufferInfo, nullptr, &buffer), "vkCreateBuffer");

    VkMemoryRequirements requirements{};
    vkGetBufferMemoryRequirements(device_, buffer, &requirements);
    VkMemoryAllocateInfo allocateInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocateInfo.allocationSize = requirements.size;
    allocateInfo.memoryTypeIndex = findMemoryType(requirements.memoryTypeBits, properties);
    vkCheck(vkAllocateMemory(device_, &allocateInfo, nullptr, &memory), "vkAllocateMemory(buffer)");
    vkCheck(vkBindBufferMemory(device_, buffer, memory, 0), "vkBindBufferMemory");
}

void AzureRenderApp::copyBuffer(
    const VkBuffer source,
    const VkBuffer destination,
    const VkDeviceSize size) const {
    VkCommandBufferAllocateInfo allocateInfo{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocateInfo.commandPool = commandPool_;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    vkCheck(
        vkAllocateCommandBuffers(device_, &allocateInfo, &commandBuffer),
        "vkAllocateCommandBuffers(copy)");
    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkCheck(vkBeginCommandBuffer(commandBuffer, &beginInfo), "vkBeginCommandBuffer(copy)");

    const VkBufferCopy region{0, 0, size};
    vkCmdCopyBuffer(commandBuffer, source, destination, 1, &region);
    vkCheck(vkEndCommandBuffer(commandBuffer), "vkEndCommandBuffer(copy)");

    VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    vkCheck(vkQueueSubmit(graphicsQueue_, 1, &submitInfo, VK_NULL_HANDLE), "vkQueueSubmit(copy)");
    vkCheck(vkQueueWaitIdle(graphicsQueue_), "vkQueueWaitIdle(copy)");
    vkFreeCommandBuffers(device_, commandPool_, 1, &commandBuffer);
}

void AzureRenderApp::transitionImageLayout(
    const VkImage image,
    const VkImageLayout oldLayout,
    const VkImageLayout newLayout) const {
    VkCommandBufferAllocateInfo allocateInfo{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocateInfo.commandPool = commandPool_;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandBufferCount = 1;
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    vkCheck(
        vkAllocateCommandBuffers(device_, &allocateInfo, &commandBuffer),
        "vkAllocateCommandBuffers(image transition)");

    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkCheck(
        vkBeginCommandBuffer(commandBuffer, &beginInfo),
        "vkBeginCommandBuffer(image transition)");

    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags sourceStage = 0;
    VkPipelineStageFlags destinationStage = 0;
    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED
        && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (
        oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
        && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        throw std::runtime_error("Unsupported image layout transition");
    }

    vkCmdPipelineBarrier(
        commandBuffer,
        sourceStage,
        destinationStage,
        0,
        0,
        nullptr,
        0,
        nullptr,
        1,
        &barrier);
    vkCheck(
        vkEndCommandBuffer(commandBuffer),
        "vkEndCommandBuffer(image transition)");
    VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    vkCheck(
        vkQueueSubmit(graphicsQueue_, 1, &submitInfo, VK_NULL_HANDLE),
        "vkQueueSubmit(image transition)");
    vkCheck(vkQueueWaitIdle(graphicsQueue_), "vkQueueWaitIdle(image transition)");
    vkFreeCommandBuffers(device_, commandPool_, 1, &commandBuffer);
}

void AzureRenderApp::copyBufferToImage(
    const VkBuffer source,
    const VkImage destination,
    const std::uint32_t width,
    const std::uint32_t height) const {
    VkCommandBufferAllocateInfo allocateInfo{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocateInfo.commandPool = commandPool_;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandBufferCount = 1;
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    vkCheck(
        vkAllocateCommandBuffers(device_, &allocateInfo, &commandBuffer),
        "vkAllocateCommandBuffers(image copy)");

    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkCheck(vkBeginCommandBuffer(commandBuffer, &beginInfo), "vkBeginCommandBuffer(image copy)");
    VkBufferImageCopy region{};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = {width, height, 1};
    vkCmdCopyBufferToImage(
        commandBuffer,
        source,
        destination,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &region);
    vkCheck(vkEndCommandBuffer(commandBuffer), "vkEndCommandBuffer(image copy)");
    VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    vkCheck(
        vkQueueSubmit(graphicsQueue_, 1, &submitInfo, VK_NULL_HANDLE),
        "vkQueueSubmit(image copy)");
    vkCheck(vkQueueWaitIdle(graphicsQueue_), "vkQueueWaitIdle(image copy)");
    vkFreeCommandBuffers(device_, commandPool_, 1, &commandBuffer);
}

void AzureRenderApp::createImage(
    const std::uint32_t width,
    const std::uint32_t height,
    const VkFormat format,
    const VkImageUsageFlags usage,
    VkImage& image,
    VkDeviceMemory& memory) const {
    VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = {width, height, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    vkCheck(vkCreateImage(device_, &imageInfo, nullptr, &image), "vkCreateImage");

    VkMemoryRequirements requirements{};
    vkGetImageMemoryRequirements(device_, image, &requirements);
    VkMemoryAllocateInfo allocateInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocateInfo.allocationSize = requirements.size;
    allocateInfo.memoryTypeIndex =
        findMemoryType(requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    vkCheck(vkAllocateMemory(device_, &allocateInfo, nullptr, &memory), "vkAllocateMemory(image)");
    vkCheck(vkBindImageMemory(device_, image, memory, 0), "vkBindImageMemory");
}

VkImageView AzureRenderApp::createImageView(
    const VkImage image,
    const VkFormat format,
    const VkImageAspectFlags aspect) const {
    VkImageViewCreateInfo createInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    createInfo.image = image;
    createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    createInfo.format = format;
    createInfo.subresourceRange.aspectMask = aspect;
    createInfo.subresourceRange.levelCount = 1;
    createInfo.subresourceRange.layerCount = 1;

    VkImageView imageView = VK_NULL_HANDLE;
    vkCheck(vkCreateImageView(device_, &createInfo, nullptr, &imageView), "vkCreateImageView");
    return imageView;
}

std::vector<char> AzureRenderApp::readBinaryFile(const std::string& path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file) {
        throw std::runtime_error("Unable to open shader: " + path);
    }
    const auto fileSize = static_cast<std::size_t>(file.tellg());
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    return buffer;
}

VkShaderModule AzureRenderApp::createShaderModule(const std::vector<char>& code) const {
    VkShaderModuleCreateInfo createInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const std::uint32_t*>(code.data());
    VkShaderModule shaderModule = VK_NULL_HANDLE;
    vkCheck(
        vkCreateShaderModule(device_, &createInfo, nullptr, &shaderModule),
        "vkCreateShaderModule");
    return shaderModule;
}

void AzureRenderApp::framebufferResizeCallback(GLFWwindow* window, int, int) {
    auto* application = static_cast<AzureRenderApp*>(glfwGetWindowUserPointer(window));
    application->framebufferResized_ = true;
}

VKAPI_ATTR VkBool32 VKAPI_CALL AzureRenderApp::debugCallback(
    const VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT,
    const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
    void*) {
    const char* prefix = severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT
        ? "validation error"
        : "validation warning";
    std::cerr << '[' << prefix << "] " << callbackData->pMessage << '\n';
    return VK_FALSE;
}

void AzureRenderApp::populateDebugMessengerCreateInfo(
    VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
    createInfo = {VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
    createInfo.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
}
