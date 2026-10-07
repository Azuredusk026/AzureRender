#include "render/LightBuffer.hpp"
#include <cassert>

int main() {
    azurerender::LightBuffer buffer;
    buffer.setLights({{9}, {2}, {5}});
    assert(buffer.lights().size() == 3);
    assert(buffer.lights()[0].stableId == 2);
    assert(buffer.lights()[2].stableId == 9);
    assert(buffer.gpuData().size() == 3);
    assert(buffer.gpuData()[0].positionRadius[3] == buffer.lights()[0].radius);

    std::vector<azurerender::RenderLight> many;
    for (std::uint64_t i = 0; i < 5; ++i) many.push_back({i});
    buffer.setLights(std::move(many), 3);
    assert(buffer.lights().size() == 3);
    assert(buffer.lights()[2].stableId == 2);
}
