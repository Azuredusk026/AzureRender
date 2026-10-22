#include "render/ClusteredLightGrid.hpp"
#include "render/LightBuffer.hpp"

#include <cassert>
#include <stdexcept>
#include <iostream>

int main() try {
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
    for(bool orthographic:{false,true})for(unsigned axis=0;axis<2;++axis)for(float sign:{-1.F,1.F}) {
        azurerender::ClusterGridDesc desc{4,4,1,.1F,10.F,axis==0?-1.F:1.F,axis==1?-1.F:1.F,orthographic};
        azurerender::ClusteredLightGrid signedProjection(desc);
        azurerender::RenderLight light;light.viewSpaceBounds=true;light.viewPosition={0,0,1};
        light.viewPosition[axis]=sign*.6F;light.clusterPosition[axis]=-sign*.6F;light.clusterPosition[2]=1;light.radius=.2F;
        signedProjection.assign({light});
        const unsigned edge=sign>0?0:3,adjacent=sign>0?1:2;
        if(signedProjection.indicesFor(axis==0?edge:2,axis==1?edge:2,0).empty()
            ||signedProjection.indicesFor(axis==0?adjacent:2,axis==1?adjacent:2,0).empty())
            throw std::runtime_error("Signed view-space projection must sort bounds and retain every touched tile");
        light.viewSpaceBounds=false;signedProjection.assign({light});
        if(signedProjection.indicesFor(axis==0?edge:2,axis==1?edge:2,0).empty())
            throw std::runtime_error("Projected centre bounds must use absolute projection radii");
    }
    azurerender::ClusteredLightGrid perspective({4, 4, 4, .1F, 100.F});
    azurerender::RenderLight behind;
    behind.viewSpaceBounds = true;
    behind.viewPosition = {0, 0, -.5F}; // Positive z is forward in this contract.
    behind.clusterPosition[2] = -.5F;
    behind.radius = 2;
    perspective.assign({behind});
    if (perspective.indicesFor(0,0,0).empty() || perspective.indicesFor(3,3,0).empty())
        throw std::runtime_error("Light behind camera must cover visible influence volume");
    behind.viewPosition = {100, 0, 10};
    behind.clusterPosition[2] = 10;
    behind.radius = 1;
    perspective.assign({behind});
    if (!perspective.gpuIndexData().empty()) throw std::runtime_error("Offscreen volume must be culled");
    behind.shape.type=static_cast<azurerender::LightKind>(2);
    behind.radius=.001F;
    behind.viewPosition={10000,10000,-10000};
    behind.clusterPosition[2]=-10000;
    perspective.assign({behind});
    for(unsigned z=0;z<4;++z)for(unsigned y=0;y<4;++y)for(unsigned x=0;x<4;++x)
        if(perspective.indicesFor(x,y,z)!=std::vector<std::uint32_t>{0})
            throw std::runtime_error("Directional light must illuminate every cluster independently of position and radius");
    return 0;
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
