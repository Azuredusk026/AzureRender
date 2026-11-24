#include "GlfwFrontend.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>

#ifdef _WIN32
#define NOMINMAX
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <commctrl.h>
#endif

namespace azurerender {
namespace {

constexpr std::array<std::pair<int, int>, 3> windowSizes{{{1280, 720}, {1600, 900}, {1920, 1080}}};

#ifdef _WIN32
constexpr UINT kTopmostCommand = 8191;
constexpr UINT_PTR kTopmostSubclassId = 1;

std::wstring wideMenuCaption(std::string_view caption) {
    if (caption.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) return {};
    const auto size = static_cast<int>(caption.size());
    const auto count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, caption.data(), size, nullptr, 0);
    if (count <= 0) return {};
    std::wstring result(count, L'\0');
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, caption.data(), size, result.data(), count) != count)
        return {};
    return result;
}

LRESULT CALLBACK topmostMenuProcedure(HWND window, UINT message, WPARAM wParam, LPARAM lParam,
                                     UINT_PTR subclass, DWORD_PTR data) {
    if (message == WM_SYSCOMMAND && wParam == kTopmostCommand) {
        *reinterpret_cast<bool*>(data) = true;
        return 0;
    }
    if (message == WM_NCDESTROY) RemoveWindowSubclass(window, topmostMenuProcedure, subclass);
    return DefSubclassProc(window, message, wParam, lParam);
}
#endif

GLFWmonitor* displayFor(GLFWwindow* window) {
    int left, top, width, height, count;
    glfwGetWindowPos(window, &left, &top);
    glfwGetWindowSize(window, &width, &height);
    auto** monitors = glfwGetMonitors(&count);
    auto* selected = glfwGetPrimaryMonitor();
    std::int64_t largest = 0;
    for (int index = 0; index < count; ++index) {
        const auto* mode = glfwGetVideoMode(monitors[index]);
        if (!mode) continue;
        int x, y;
        glfwGetMonitorPos(monitors[index], &x, &y);
        const auto overlapWidth = std::max(0, std::min(left + width, x + mode->width) - std::max(left, x));
        const auto overlapHeight = std::max(0, std::min(top + height, y + mode->height) - std::max(top, y));
        const auto area = static_cast<std::int64_t>(overlapWidth) * overlapHeight;
        if (area > largest) {
            largest = area;
            selected = monitors[index];
        }
    }
    return selected;
}

} // namespace

GlfwFrontend::GlfwFrontend(const GlfwFrontendConfig& config) {
    if (glfwInit() != GLFW_TRUE) {
        throw std::runtime_error("GLFW initialization failed");
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, config.resizable ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_VISIBLE, config.visible ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_DECORATED, config.decorated ? GLFW_TRUE : GLFW_FALSE);
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
    glfwSetScrollCallback(window_, config.scrollCallback);
    glfwSetDropCallback(window_, config.dropCallback);
}

GlfwFrontend::~GlfwFrontend() {
    setDisplayAwake(false);
    if (window_ != nullptr) {
        setTopmostMenu(false);
        glfwDestroyWindow(window_);
    }
    glfwTerminate();
}

bool GlfwFrontend::shouldClose() const {
    return glfwWindowShouldClose(window_) == GLFW_TRUE;
}

bool GlfwFrontend::isForeground() const {
    if (glfwGetWindowAttrib(window_, GLFW_VISIBLE) != GLFW_TRUE
        || glfwGetWindowAttrib(window_, GLFW_ICONIFIED) == GLFW_TRUE) return false;
#ifdef _WIN32
    return glfwGetWin32Window(window_) == GetForegroundWindow();
#else
    return glfwGetWindowAttrib(window_, GLFW_FOCUSED) == GLFW_TRUE;
#endif
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

void GlfwFrontend::applyWindowPresentation(WindowMode mode, WindowResolution resolution, bool alwaysOnTop) {
    auto preset = std::pair{0, 0};
    if (resolution != WindowResolution::Custom) preset = windowSizes.at(static_cast<std::size_t>(resolution));
    if (mode == WindowMode::Borderless) {
        auto* monitor = displayFor(window_);
        const auto* current = monitor ? glfwGetVideoMode(monitor) : nullptr;
        if (!current) throw std::runtime_error("Borderless mode requires an available monitor");
        if (!windowedPlacement_) {
            WindowedPlacement saved;
            saved.decorated = glfwGetWindowAttrib(window_, GLFW_DECORATED);
            saved.resizable = glfwGetWindowAttrib(window_, GLFW_RESIZABLE);
            saved.maximized = glfwGetWindowAttrib(window_, GLFW_MAXIMIZED) == GLFW_TRUE;
            if (saved.maximized) glfwRestoreWindow(window_);
            glfwGetWindowPos(window_, &saved.x, &saved.y);
            glfwGetWindowSize(window_, &saved.width, &saved.height);
            windowedPlacement_ = saved;
        }
        int x, y;
        glfwGetMonitorPos(monitor, &x, &y);
        glfwSetWindowAttrib(window_, GLFW_DECORATED, GLFW_FALSE);
        glfwSetWindowAttrib(window_, GLFW_RESIZABLE, GLFW_FALSE);
        glfwSetWindowPos(window_, x, y);
        glfwSetWindowSize(window_, current->width, current->height);
    } else if (windowedPlacement_) {
        const auto saved = *windowedPlacement_;
        glfwSetWindowAttrib(window_, GLFW_DECORATED, saved.decorated);
        glfwSetWindowAttrib(window_, GLFW_RESIZABLE, saved.resizable);
        glfwSetWindowPos(window_, saved.x, saved.y);
        if (resolution == WindowResolution::Custom) {
            glfwSetWindowSize(window_, saved.width, saved.height);
            if (saved.maximized) glfwMaximizeWindow(window_);
        } else {
            glfwSetWindowSize(window_, preset.first, preset.second);
        }
        windowedPlacement_.reset();
    } else if (resolution != WindowResolution::Custom) {
        if (glfwGetWindowAttrib(window_, GLFW_MAXIMIZED)) glfwRestoreWindow(window_);
        glfwSetWindowSize(window_, preset.first, preset.second);
    }
    const auto floating = alwaysOnTop ? GLFW_TRUE : GLFW_FALSE;
    if (glfwGetWindowAttrib(window_, GLFW_FLOATING) != floating)
        glfwSetWindowAttrib(window_, GLFW_FLOATING, floating);
    resolution_ = resolution;
}

WindowPresentation GlfwFrontend::windowPresentation() {
    if (!windowedPlacement_ && resolution_ != WindowResolution::Custom) {
        const auto size = framebufferSize();
        if (size.first > 0 && size.second > 0 && size != windowSizes.at(static_cast<std::size_t>(resolution_)))
            resolution_ = WindowResolution::Custom;
    }
    return {windowedPlacement_ ? WindowMode::Borderless : WindowMode::Windowed, resolution_,
        glfwGetWindowAttrib(window_, GLFW_FLOATING) == GLFW_TRUE};
}

void GlfwFrontend::setTopmostMenu(bool enabled, std::string_view caption) {
#ifdef _WIN32
    const bool topmost = glfwGetWindowAttrib(window_, GLFW_FLOATING) == GLFW_TRUE;
    if (caption.empty()) caption = topmost ? "Always on Top(Off)" : "Always on Top(On)";
    if (enabled == topmostMenuEnabled_ && (!enabled || std::string_view(topmostMenuCaption_) == caption)) return;
    const auto window = glfwGetWin32Window(window_);
    const auto menu = GetSystemMenu(window, FALSE);
    if (!enabled) {
        RemoveWindowSubclass(window, topmostMenuProcedure, kTopmostSubclassId);
        if (menu) RemoveMenu(menu, kTopmostCommand, MF_BYCOMMAND);
        topmostMenuEnabled_ = false;
        topmostMenuToggle_ = false;
        topmostMenuCaption_.clear();
        return;
    }
    auto title = wideMenuCaption(caption);
    if (title.empty()) title = topmost ? L"Always on Top(Off)" : L"Always on Top(On)";
    if (topmostMenuEnabled_) {
        if (ModifyMenuW(menu, kTopmostCommand, MF_BYCOMMAND | MF_STRING, kTopmostCommand, title.c_str()))
            topmostMenuCaption_ = caption;
        return;
    }
    if (!menu || !SetWindowSubclass(window, topmostMenuProcedure, kTopmostSubclassId,
                                   reinterpret_cast<DWORD_PTR>(&topmostMenuToggle_))) return;
    if (!InsertMenuW(menu, 0, MF_BYPOSITION | MF_STRING, kTopmostCommand, title.c_str())) {
        RemoveWindowSubclass(window, topmostMenuProcedure, kTopmostSubclassId);
        return;
    }
    topmostMenuEnabled_ = true;
    topmostMenuCaption_ = caption;
#else
    (void)enabled;
    (void)caption;
#endif
}

bool GlfwFrontend::consumeTopmostMenuToggle() {
    return std::exchange(topmostMenuToggle_, false);
}

void GlfwFrontend::setDisplayAwake(bool enabled) {
#ifdef _WIN32
    if (enabled == keepDisplayAwake_) return;
    keepDisplayAwake_ = enabled;
    if (!enabled) {
        if (const auto request = std::exchange(displayPowerRequest_, nullptr)) {
            PowerClearRequest(request, PowerRequestDisplayRequired);
            CloseHandle(request);
        }
        return;
    }

    wchar_t reason[] = L"AzureRender room display";
    REASON_CONTEXT context{};
    context.Version = POWER_REQUEST_CONTEXT_VERSION;
    context.Flags = POWER_REQUEST_CONTEXT_SIMPLE_STRING;
    context.Reason.SimpleReasonString = reason;
    const auto request = PowerCreateRequest(&context);
    if (request == INVALID_HANDLE_VALUE) return;
    if (PowerSetRequest(request, PowerRequestDisplayRequired)) displayPowerRequest_ = request;
    else CloseHandle(request);
#else
    (void)enabled;
#endif
}

bool GlfwFrontend::updatePresentationSync(bool synchronized) {
    return std::exchange(synchronizedPresentation_, synchronized) != synchronized;
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
void GlfwFrontend::recordFileDrop(int count,const char** paths) {
    if(count<=0||count>32||fileDrops_.size()>=32)return;
    FileDropEvent event;glfwGetCursorPos(window_,&event.x,&event.y);
    for(int i=0;i<count;++i)event.paths.push_back(std::filesystem::u8path(paths[i]));
    fileDrops_.push_back(std::move(event));
}

void GlfwFrontend::waitEvents() const {
    glfwWaitEvents();
}
void GlfwFrontend::waitEventsTimeout(double timeoutSeconds) const {
    glfwWaitEventsTimeout(timeoutSeconds);
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
