#include "Window.hpp"
#include <algorithm>
#include <chrono>
#include <stdexcept>
#if defined(THESIS_WSI)
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#endif
namespace thesis {
static void requireSuccess(VkResult result,const char* name) {
    if(result!=VK_SUCCESS && result!=VK_SUBOPTIMAL_KHR) throw std::runtime_error(name);
}
Window::Window(uint32_t width,uint32_t height) {
#if defined(THESIS_WSI)
    if(!glfwInit()) throw std::runtime_error("GLFW initialization failed");
    glfwWindowHint(GLFW_CLIENT_API,GLFW_NO_API); glfwWindowHint(GLFW_VISIBLE,GLFW_FALSE);
    window=glfwCreateWindow(int(std::min(width,640u)),int(std::min(height,480u)),"AzureThesis swapchain",nullptr,nullptr);
    if(!window) { glfwTerminate(); throw std::runtime_error("hidden benchmark window creation failed"); }
#else
    (void)width;(void)height;
    throw std::runtime_error("swapchain support was not configured");
#endif
}
std::vector<const char*> Window::instanceExtensions() {
#if defined(THESIS_WSI)
    uint32_t count=0;auto names=glfwGetRequiredInstanceExtensions(&count);
    if(!names || !count) throw std::runtime_error("GLFW Vulkan extensions unavailable");
    return {names,names+count};
#else
    return {};
#endif
}
void Window::createSurface(VkInstance value) {
    instance=value;
#if defined(THESIS_WSI)
    requireSuccess(glfwCreateWindowSurface(instance,static_cast<GLFWwindow*>(window),nullptr,&surface),"surface creation failed");
#endif
}
bool Window::supports(VkPhysicalDevice physical,uint32_t family) {
    VkBool32 supported=VK_FALSE;
    requireSuccess(vkGetPhysicalDeviceSurfaceSupportKHR(physical,family,surface,&supported),"presentation support query failed");
    return supported==VK_TRUE;
}
void Window::initialize(VkPhysicalDevice physical,VkDevice value,uint32_t family) {
    device=value;
    VkSurfaceCapabilitiesKHR caps{};requireSuccess(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical,surface,&caps),"surface capabilities failed");
    if(!(caps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT)) throw std::runtime_error("swapchain transfer destination unsupported");
    uint32_t count=0;requireSuccess(vkGetPhysicalDeviceSurfaceFormatsKHR(physical,surface,&count,nullptr),"surface formats failed");
    std::vector<VkSurfaceFormatKHR> formats(count);requireSuccess(vkGetPhysicalDeviceSurfaceFormatsKHR(physical,surface,&count,formats.data()),"surface formats failed");
    VkSurfaceFormatKHR selected{};
    for(auto f:formats) if(f.format==VK_FORMAT_B8G8R8A8_UNORM || f.format==VK_FORMAT_R8G8B8A8_UNORM) { selected=f;break; }
    if(!selected.format) throw std::runtime_error("unorm swapchain format unavailable");
    VkFormatProperties source{},destination{};
    vkGetPhysicalDeviceFormatProperties(physical,VK_FORMAT_R8G8B8A8_UNORM,&source);
    vkGetPhysicalDeviceFormatProperties(physical,selected.format,&destination);
    if(!(source.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_SRC_BIT) || !(destination.optimalTilingFeatures & VK_FORMAT_FEATURE_BLIT_DST_BIT)) throw std::runtime_error("presentation blit unsupported");
    extent=caps.currentExtent;
    if(extent.width==UINT32_MAX) extent={std::clamp(640u,caps.minImageExtent.width,caps.maxImageExtent.width),std::clamp(480u,caps.minImageExtent.height,caps.maxImageExtent.height)};
    count=std::max(2u,caps.minImageCount); if(caps.maxImageCount) count=std::min(count,caps.maxImageCount);
    VkSwapchainCreateInfoKHR ci{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR}; ci.surface=surface;ci.minImageCount=count;
    ci.imageFormat=selected.format;ci.imageColorSpace=selected.colorSpace;ci.imageExtent=extent;ci.imageArrayLayers=1;
    ci.imageUsage=VK_IMAGE_USAGE_TRANSFER_DST_BIT;ci.imageSharingMode=VK_SHARING_MODE_EXCLUSIVE;ci.preTransform=caps.currentTransform;
    for(uint32_t bit=1;bit<=VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;bit<<=1) if(caps.supportedCompositeAlpha & bit) { ci.compositeAlpha=VkCompositeAlphaFlagBitsKHR(bit);break; }
    ci.presentMode=VK_PRESENT_MODE_FIFO_KHR;ci.clipped=VK_TRUE;
    requireSuccess(vkCreateSwapchainKHR(device,&ci,nullptr,&swapchain),"swapchain creation failed");
    requireSuccess(vkGetSwapchainImagesKHR(device,swapchain,&count,nullptr),"swapchain images failed");images.resize(count);
    requireSuccess(vkGetSwapchainImagesKHR(device,swapchain,&count,images.data()),"swapchain images failed");
    VkSemaphoreCreateInfo si{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    requireSuccess(vkCreateSemaphore(device,&si,nullptr,&acquired),"acquire semaphore failed");
    complete.resize(images.size());
    for(auto& semaphore:complete) requireSuccess(vkCreateSemaphore(device,&si,nullptr,&semaphore),"present semaphore failed");
    VkCommandPoolCreateInfo pi{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};pi.queueFamilyIndex=family;
    requireSuccess(vkCreateCommandPool(device,&pi,nullptr,&pool),"presentation command pool failed");
    VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};ai.commandPool=pool;ai.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;ai.commandBufferCount=1;
    requireSuccess(vkAllocateCommandBuffers(device,&ai,&command),"presentation command allocation failed");
}
double Window::present(VkQueue queue,VkImage image,uint32_t width,uint32_t height) {
    auto start=std::chrono::steady_clock::now();uint32_t index=0;
    requireSuccess(vkAcquireNextImageKHR(device,swapchain,5'000'000'000ull,acquired,VK_NULL_HANDLE,&index),"swapchain acquisition failed");
    requireSuccess(vkResetCommandPool(device,pool,0),"presentation command reset failed");
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};requireSuccess(vkBeginCommandBuffer(command,&begin),"presentation recording failed");
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};barrier.oldLayout=VK_IMAGE_LAYOUT_UNDEFINED;barrier.newLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.dstAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;barrier.srcQueueFamilyIndex=barrier.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;
    barrier.image=images[index];barrier.subresourceRange={VK_IMAGE_ASPECT_COLOR_BIT,0,1,0,1};
    vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,0,0,nullptr,0,nullptr,1,&barrier);
    VkImageBlit blit{};blit.srcSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};blit.dstSubresource=blit.srcSubresource;
    blit.srcOffsets[1]={int32_t(width),int32_t(height),1};blit.dstOffsets[1]={int32_t(extent.width),int32_t(extent.height),1};
    vkCmdBlitImage(command,image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,images[index],VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1,&blit,VK_FILTER_NEAREST);
    barrier.oldLayout=VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;barrier.newLayout=VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    barrier.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;barrier.dstAccessMask=0;
    vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,0,0,nullptr,0,nullptr,1,&barrier);
    requireSuccess(vkEndCommandBuffer(command),"presentation recording end failed");
    VkPipelineStageFlags stage=VK_PIPELINE_STAGE_TRANSFER_BIT;
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.waitSemaphoreCount=1;submit.pWaitSemaphores=&acquired;submit.pWaitDstStageMask=&stage;
    submit.commandBufferCount=1;submit.pCommandBuffers=&command;submit.signalSemaphoreCount=1;submit.pSignalSemaphores=&complete[index];
    requireSuccess(vkQueueSubmit(queue,1,&submit,VK_NULL_HANDLE),"presentation submit failed");
    VkPresentInfoKHR p{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};p.waitSemaphoreCount=1;p.pWaitSemaphores=&complete[index];p.swapchainCount=1;p.pSwapchains=&swapchain;p.pImageIndices=&index;
    requireSuccess(vkQueuePresentKHR(queue,&p),"presentation failed");requireSuccess(vkQueueWaitIdle(queue),"presentation completion failed");
#if defined(THESIS_WSI)
    glfwPollEvents();
#endif
    return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
}
Window::~Window() {
    if(device) { if(pool)vkDestroyCommandPool(device,pool,nullptr);if(acquired)vkDestroySemaphore(device,acquired,nullptr);for(auto s:complete)if(s)vkDestroySemaphore(device,s,nullptr);if(swapchain)vkDestroySwapchainKHR(device,swapchain,nullptr); }
    if(surface)vkDestroySurfaceKHR(instance,surface,nullptr);
#if defined(THESIS_WSI)
    if(window)glfwDestroyWindow(static_cast<GLFWwindow*>(window));glfwTerminate();
#endif
}
}
