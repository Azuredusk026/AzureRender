#pragma once

namespace azurerender {

template <typename Frontend>
bool waitForDrawableSurface(Frontend& frontend) {
    while (!frontend.shouldClose()) {
        const auto size = frontend.framebufferSize();
        if (size.first > 0 && size.second > 0) return true;
        frontend.waitEvents();
    }
    return false;
}

}  // namespace azurerender
