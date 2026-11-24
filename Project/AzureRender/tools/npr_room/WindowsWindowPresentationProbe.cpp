#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "platform/GlfwFrontend.hpp"
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

#include <iostream>
#include <stdexcept>

namespace {

void check(bool passed, const char* message) {
    if (!passed) throw std::runtime_error(message);
}

std::pair<int, int> client(HWND window) {
    RECT rect{};
    check(GetClientRect(window, &rect) != FALSE, "Windows client rectangle is unavailable");
    return {rect.right - rect.left, rect.bottom - rect.top};
}

bool topmost(HWND window) {
    return (GetWindowLongPtrW(window, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0;
}

} // namespace

int main() try {
    using namespace azurerender;
    GlfwFrontendConfig config;
    config.title = "AzureRender window presentation probe";
    config.width = 1111;
    config.height = 701;
    config.visible = false;
    GlfwFrontend frontend(config);
    const auto* ignored = SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    static_cast<void>(ignored);
    const auto window = frontend.nativeHandle();
    const auto native = glfwGetWin32Window(window);
    glfwSetWindowPos(window, 73, 91);
    frontend.pollEvents();
    int left, top;
    glfwGetWindowPos(window, &left, &top);
    const auto original = client(native);
    const auto monitor = MonitorFromWindow(native, MONITOR_DEFAULTTONEAREST);
    MONITORINFO display{};
    display.cbSize = sizeof(display);
    check(GetMonitorInfoW(monitor, &display) != FALSE, "Monitor geometry is unavailable");
    const auto rect = display.rcMonitor;

    frontend.applyWindowPresentation(WindowMode::Borderless, WindowResolution::FullHD, true);
    frontend.pollEvents();
    int x, y;
    glfwGetWindowPos(window, &x, &y);
    check(x == rect.left && y == rect.top, "Borderless window must cover the selected monitor");
    check(client(native) == std::pair<int, int>{rect.right - rect.left, rect.bottom - rect.top},
          "Borderless client size must match the display");
    check(glfwGetWindowMonitor(window) == nullptr && !glfwGetWindowAttrib(window, GLFW_DECORATED),
          "Borderless mode must stay windowed with hidden decorations");
    check(!glfwGetWindowAttrib(window, GLFW_RESIZABLE) && topmost(native), "Borderless placement must honor topmost");

    frontend.applyWindowPresentation(WindowMode::Borderless, WindowResolution::HD, false);
    frontend.pollEvents();
    check(client(native) == std::pair<int, int>{rect.right - rect.left, rect.bottom - rect.top} && !topmost(native),
          "Changing topmost must preserve the borderless client size");
    frontend.applyWindowPresentation(WindowMode::Windowed, WindowResolution::Custom, false);
    frontend.pollEvents();
    glfwGetWindowPos(window, &x, &y);
    check(x == left && y == top && client(native) == original, "Custom placement must survive a fullscreen round trip");
    check(glfwGetWindowAttrib(window, GLFW_DECORATED) && glfwGetWindowAttrib(window, GLFW_RESIZABLE),
          "Windowed framing must be restored");

    frontend.applyWindowPresentation(WindowMode::Windowed, WindowResolution::HD, false);
    frontend.pollEvents();
    check(client(native) == std::pair<int, int>{1280, 720}, "1280 x 720 preset has an incorrect client size");
    frontend.applyWindowPresentation(WindowMode::Windowed, WindowResolution::HDPlus, true);
    frontend.pollEvents();
    check(client(native) == std::pair<int, int>{1600, 900} && topmost(native), "1600 x 900 and topmost failed");
    frontend.applyWindowPresentation(WindowMode::Windowed, WindowResolution::FullHD, false);
    frontend.pollEvents();
    check(client(native) == std::pair<int, int>{1920, 1080} && !topmost(native), "1920 x 1080 and topmost reset failed");
    std::cout << "Window presentation and client pixel sizes verified\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
