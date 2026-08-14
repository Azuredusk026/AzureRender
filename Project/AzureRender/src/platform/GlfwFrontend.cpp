#include "GlfwFrontend.hpp"

#include <stdexcept>

namespace azurerender {

GlfwFrontend::GlfwFrontend(const GlfwFrontendConfig& config) {
    if (glfwInit() != GLFW_TRUE) {
        throw std::runtime_error("GLFW initialization failed");
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, config.resizable ? GLFW_TRUE : GLFW_FALSE);
    window_ = glfwCreateWindow(
        static_cast<int>(config.width),
        static_cast<int>(config.height),
        config.title.c_str(),
        nullptr,
        nullptr);
    if (window_ == nullptr) {
        glfwTerminate();
        throw std::runtime_error("GLFW window creation failed");
    }
    glfwSetWindowUserPointer(window_, config.userPointer);
    glfwSetFramebufferSizeCallback(window_, config.framebufferSizeCallback);
    glfwSetKeyCallback(window_, config.keyCallback);
}

GlfwFrontend::~GlfwFrontend() {
    if (window_ != nullptr) {
        glfwDestroyWindow(window_);
    }
    glfwTerminate();
}

bool GlfwFrontend::shouldClose() const {
    return glfwWindowShouldClose(window_) == GLFW_TRUE;
}

double GlfwFrontend::timeSeconds() const {
    return glfwGetTime();
}

std::pair<int, int> GlfwFrontend::framebufferSize() const {
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(window_, &width, &height);
    return {width, height};
}

std::vector<const char*> GlfwFrontend::requiredVulkanExtensions() const {
    std::uint32_t count = 0;
    const char** extensions = glfwGetRequiredInstanceExtensions(&count);
    if (extensions == nullptr) {
        throw std::runtime_error(
            "GLFW did not provide required Vulkan instance extensions");
    }
    return {extensions, extensions + count};
}

void GlfwFrontend::pollEvents() const {
    glfwPollEvents();
}

void GlfwFrontend::waitEvents() const {
    glfwWaitEvents();
}

void GlfwFrontend::requestClose() const {
    glfwSetWindowShouldClose(window_, GLFW_TRUE);
}

VkSurfaceKHR GlfwFrontend::createSurface(const VkInstance instance) const {
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (glfwCreateWindowSurface(instance, window_, nullptr, &surface)
        != VK_SUCCESS) {
        throw std::runtime_error("glfwCreateWindowSurface failed");
    }
    return surface;
}

}  // namespace azurerender
