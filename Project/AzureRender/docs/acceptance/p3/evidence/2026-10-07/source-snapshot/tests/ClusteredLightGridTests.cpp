#include "render/ClusteredLightGrid.hpp"
#include "render/LightBuffer.hpp"

#include <cassert>

int main() {
    azurerender::ClusteredLightGrid grid({2, 2, 2, 4.0F, 32.0F});
    azurerender::RenderLight first{};
    first.stableId = 10;
    first.clusterPosition[0] = -0.75F;
    first.clusterPosition[1] = -0.75F;
    first.clusterPosition[2] = 8.0F;
    first.radius = 0.1F;
    azurerender::RenderLight second{};
    second.stableId = 20;
    second.clusterPosition[0] = 0.75F;
    second.clusterPosition[1] = 0.75F;
    second.clusterPosition[2] = 24.0F;
    second.radius = 0.1F;
    grid.assign({first, second});
    assert(grid.clusterCount() == 8);
    assert(grid.indicesFor(0, 0, 0).size() == 1);
    assert(grid.indicesFor(1, 1, 1).size() == 1);
    assert(grid.indicesFor(0, 0, 0)[0] == 0);
    assert(grid.indicesFor(1, 1, 1)[0] == 1);
    assert(grid.gpuIndexData().size() == 2);

    azurerender::RenderLight broad{};
    broad.stableId = 30;
    broad.clusterPosition[0] = 0.0F;
    broad.clusterPosition[1] = 0.0F;
    broad.clusterPosition[2] = 8.0F;
    broad.radius = 4.0F;
    grid.assign({broad});
    assert(grid.indicesFor(0, 0, 0).size() == 1);
    assert(grid.indicesFor(1, 1, 1).size() == 1);
    assert(grid.gpuIndexData().size() > 1);
    return 0;
}
