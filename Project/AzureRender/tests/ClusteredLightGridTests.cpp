#include "render/ClusteredLightGrid.hpp"
#include "render/LightBuffer.hpp"

#include <cassert>
#include <stdexcept>

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
    // Orthographic lights cover the same screen area at every depth.
    azurerender::ClusterGridDesc orthoDesc{4, 1, 1, 0.1F, 100.0F, 1.0F, 1.0F};
    orthoDesc.orthographic = true;
    azurerender::ClusteredLightGrid ortho(orthoDesc);
    first.clusterPosition[0] = -0.45F;
    first.clusterPosition[1] = 0.0F;
    first.clusterPosition[2] = 40.0F;
    first.radius = 0.2F;
    ortho.assign({first});
    if (ortho.indicesFor(0, 0, 0).size() != 1
        || ortho.indicesFor(1, 0, 0).size() != 1
        || !ortho.indicesFor(2, 0, 0).empty()) {
        throw std::runtime_error("Orthographic light footprint must be independent of depth");
    }
    first.clusterPosition[2] = 2.0F;
    ortho.assign({first});
    if (ortho.indicesFor(0, 0, 0).size() != 1
        || ortho.indicesFor(1, 0, 0).size() != 1) {
        throw std::runtime_error("Orthographic near and far lights must occupy identical screen tiles");
    }
    return 0;
}
