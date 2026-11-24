#pragma once

#include <GLFW/glfw3.h>

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "platform/FileDropEvent.hpp"

namespace azurerender {

enum class WindowMode { Windowed, Borderless };
enum class WindowResolution { HD, HDPlus, FullHD, Custom };

struct WindowPresentation {
    WindowMode mode;
    WindowResolution resolution;
    bool alwaysOnTop;
};

struct GlfwFrontendConfig {
    std::uint32_t width = 1280;
    std::uint32_t height = 720;
    std::string title = "AzureRender";
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
    [[nodiscard]] bool isForeground() const;
    [[nodiscard]] double timeSeconds() const;
    [[nodiscard]] std::pair<int, int> framebufferSize() const;
    [[nodiscard]] std::vector<const char*> requiredVulkanExtensions() const;
    void applyWindowPresentation(WindowMode mode, WindowResolution resolution, bool alwaysOnTop);
    [[nodiscard]] WindowPresentation windowPresentation();
    void setTopmostMenu(bool enabled, std::string_view caption = {});
    [[nodiscard]] bool consumeTopmostMenuToggle();
    void setDisplayAwake(bool enabled);
    [[nodiscard]] bool displayWakeRequested() const { return displayPowerRequest_ != nullptr; }
    bool updatePresentationSync(bool synchronized);

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
    struct WindowedPlacement {
        int x = 0, y = 0, width = 0, height = 0;
        int decorated = GLFW_TRUE, resizable = GLFW_TRUE;
        bool maximized = false;
    };
    std::optional<WindowedPlacement> windowedPlacement_;
    WindowResolution resolution_ = WindowResolution::Custom;
    bool topmostMenuEnabled_ = false;
    bool topmostMenuToggle_ = false;
    std::string topmostMenuCaption_;
    bool keepDisplayAwake_ = false;
    void* displayPowerRequest_ = nullptr;
    bool synchronizedPresentation_ = true;
};

}  // namespace azurerender
