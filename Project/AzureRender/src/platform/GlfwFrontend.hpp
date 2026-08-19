#pragma once

#include <GLFW/glfw3.h>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace azurerender {

struct GlfwFrontendConfig {
    std::uint32_t width = 1280;
    std::uint32_t height = 720;
    std::string title;
    bool resizable = true;
    bool visible = true;
    bool decorated = true;
    void* userPointer = nullptr;
    GLFWframebuffersizefun framebufferSizeCallback = nullptr;
    GLFWkeyfun keyCallback = nullptr;
};

class GlfwFrontend final {
public:
    explicit GlfwFrontend(const GlfwFrontendConfig& config);
    GlfwFrontend(const GlfwFrontend&) = delete;
    GlfwFrontend& operator=(const GlfwFrontend&) = delete;
    ~GlfwFrontend();

    [[nodiscard]] GLFWwindow* nativeHandle() const { return window_; }
    [[nodiscard]] bool shouldClose() const;
    [[nodiscard]] double timeSeconds() const;
    [[nodiscard]] std::pair<int, int> framebufferSize() const;
    [[nodiscard]] std::vector<const char*> requiredVulkanExtensions() const;

    void pollEvents() const;
    void waitEvents() const;
    void requestClose() const;
    VkSurfaceKHR createSurface(VkInstance instance) const;

private:
    GLFWwindow* window_ = nullptr;
};

}  // namespace azurerender
