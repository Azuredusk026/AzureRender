#pragma once
#include <vulkan/vulkan.h>
#include <vector>
namespace thesis {
class Window {
public:
    Window(uint32_t width,uint32_t height);
    ~Window();
    std::vector<const char*> instanceExtensions();
    void createSurface(VkInstance instance);
    bool supports(VkPhysicalDevice physical,uint32_t family);
    void initialize(VkPhysicalDevice physical,VkDevice device,uint32_t family);
    double present(VkQueue queue,VkImage image,uint32_t width,uint32_t height);
private:
    void* window{};
    VkInstance instance{};
    VkSurfaceKHR surface{};
    VkDevice device{};
    VkSwapchainKHR swapchain{};
    std::vector<VkImage> images;
    VkExtent2D extent{};
    VkSemaphore acquired{};
    std::vector<VkSemaphore> complete;
    VkCommandPool pool{};
    VkCommandBuffer command{};
};
}
