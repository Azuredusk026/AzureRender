#pragma once

namespace azurerender {

template <typename Frontend,typename IdleWork>
bool waitForDrawableSurface(Frontend& frontend,IdleWork&& idleWork) {
    while (!frontend.shouldClose()) {
        const auto size = frontend.framebufferSize();
        if (size.first > 0 && size.second > 0) return true;
        if(!idleWork()||frontend.shouldClose())return false;
        // Host services must remain responsive without advancing a frame.
        // The native event wait also processes restoration and close events.
        frontend.waitEventsTimeout(.05);
    }
    return false;
}
template <typename Frontend>
bool waitForDrawableSurface(Frontend& frontend) {
    return waitForDrawableSurface(frontend,[]{return true;});
}

}  // namespace azurerender
