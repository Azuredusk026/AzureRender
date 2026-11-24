#include "platform/SurfaceLifecycle.hpp"

#include <stdexcept>
#include <utility>

struct Frontend {
    std::pair<int, int> size{1280, 720};
    int waits = 0;
    bool closed = false;
    bool closeOnWait = false;
    auto framebufferSize() const { return size; }
    bool shouldClose() const { return closed; }
    void waitEvents() {
        if (++waits > 2) throw std::runtime_error("waiting indefinitely");
        if (closeOnWait) closed = true;
        else size = {1280, 720};
    }
};

int main() {
    Frontend ready;
    if (!azurerender::waitForDrawableSurface(ready) || ready.waits != 0)
        return 1;
    Frontend minimized;
    minimized.size = {0, 0};
    if (!azurerender::waitForDrawableSurface(minimized) || minimized.waits != 1)
        return 2;
    Frontend closing;
    closing.size = {0, 0};
    closing.closeOnWait = true;
    if (azurerender::waitForDrawableSurface(closing) || closing.waits != 1)
        return 3;
    Frontend closed;
    closed.closed = true;
    if (azurerender::waitForDrawableSurface(closed) || closed.waits != 0)
        return 4;
}
