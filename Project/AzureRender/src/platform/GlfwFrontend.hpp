#pragma once

#include <GLFW/glfw3.h>

#include <cstdint>
#include <string>
#include <utility>
#include <vector>
#include "platform/FileDropEvent.hpp"

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
    GLFWscrollfun scrollCallback = nullptr;
    GLFWdropfun dropCallback = nullptr;
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
    void recordFileDrop(int count,const char** paths);
    std::vector<FileDropEvent> consumeFileDrops(){auto result=std::move(fileDrops_);fileDrops_.clear();return result;}
    void waitEvents() const;
    void waitEventsTimeout(double timeoutSeconds) const;
    void requestClose() const;
    VkSurfaceKHR createSurface(VkInstance instance) const;

private:
    GLFWwindow* window_ = nullptr;
    std::vector<FileDropEvent> fileDrops_;
};

}  // namespace azurerender
