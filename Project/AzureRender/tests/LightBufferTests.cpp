#include "render/LightBuffer.hpp"
#include <cassert>

int main() {
    azurerender::LightBuffer buffer;
    buffer.setLights({{9}, {2}, {5}});
    assert(buffer.lights().size() == 3);
    assert(buffer.lights()[0].stableId == 2);
    assert(buffer.lights()[2].stableId == 9);
}
