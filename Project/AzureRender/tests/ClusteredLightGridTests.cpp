#include "render/ClusteredLightGrid.hpp"
#include "render/LightBuffer.hpp"

#include <cassert>

int main() {
    azurerender::ClusteredLightGrid grid({2, 2, 2, 4.0F, 32.0F});
    azurerender::RenderLight first{};
    first.stableId = 10;
    first.position[0] = -0.75F;
    first.position[1] = -0.75F;
    first.position[2] = 8.0F;
    azurerender::RenderLight second{};
    second.stableId = 20;
    second.position[0] = 0.75F;
    second.position[1] = 0.75F;
    second.position[2] = 24.0F;
    grid.assign({first, second});
    assert(grid.clusterCount() == 8);
    assert(grid.indicesFor(0, 0, 0).size() == 1);
    assert(grid.indicesFor(1, 1, 1).size() == 1);
    assert(grid.indicesFor(0, 0, 0)[0] == 0);
    assert(grid.indicesFor(1, 1, 1)[0] == 1);
    assert(grid.gpuIndexData().size() == 2);
    return 0;
}
