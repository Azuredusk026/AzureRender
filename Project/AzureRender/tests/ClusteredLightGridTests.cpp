#include "render/ClusteredLightGrid.hpp"
#include "render/LightBuffer.hpp"

#include <cassert>
#include <stdexcept>
#include <iostream>
#include <chrono>
#include <limits>

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
    behind.shape.type=azurerender::LightKind::Spot;behind.shape.falloff=azurerender::LightFalloff::RangePower;
    auto& modifier=behind.shape.characterResponse;modifier.present=true;modifier.schemaVersion=3;
    modifier.projectedDistance=true;modifier.strength=.7F;perspective.assign({behind});
    for(unsigned z=0;z<4;++z)for(unsigned y=0;y<4;++y)for(unsigned x=0;x<4;++x)
        if(perspective.indicesFor(x,y,z)!=std::vector<std::uint32_t>{0})
            throw std::runtime_error("Projected color slab must retain conservative cluster coverage");
    for(bool ortho:{false,true})for(float sign:{-1.F,1.F}) {
        azurerender::ClusterGridDesc shifted{4,4,1,.1F,10,sign,-1,ortho};shifted.projectionOffsetX=.75F;shifted.projectionOffsetY=-.5F;
        azurerender::ClusteredLightGrid offsetGrid(shifted);azurerender::RenderLight local;
        local.radius=.05F;local.viewSpaceBounds=true;local.viewPosition={sign*(ortho?-.5F:-1.F),0,2};
        local.clusterPosition[0]=.25F;local.clusterPosition[1]=-.5F;local.clusterPosition[2]=2;
        offsetGrid.assign({local});const auto indexed=offsetGrid.gpuHeaderData();
        if(offsetGrid.indicesFor(2,0,0).empty()||offsetGrid.indicesFor(2,1,0).empty()||!offsetGrid.indicesFor(0,0,0).empty())
            throw std::runtime_error("Offset camera lights must cover their translated viewport tiles");
        local.viewSpaceBounds=false;offsetGrid.assign({local});
        if(indexed!=offsetGrid.gpuHeaderData())throw std::runtime_error("Fallback NDC bounds must consume the offset exactly once");
    }
    {
        azurerender::ClusteredLightGrid reused({16,9,24});
        for(bool orthographic:{false,true})for(unsigned x:{16U,8U,16U}){
            azurerender::ClusterGridDesc desc{x,9,24,.2F,120.F,1.5F,2.F,orthographic,.125F,-.125F};
            azurerender::ClusteredLightGrid fresh(desc);
            auto light=behind;light.shape.type=azurerender::LightKind::Directional;
            reused.configure(desc);reused.assign({first,second,light});fresh.assign({first,second,light});
            if(reused.gpuHeaderData()!=fresh.gpuHeaderData()||reused.gpuIndexData()!=fresh.gpuIndexData())
                throw std::runtime_error("Reconfigured storage must preserve fresh-grid GPU bytes and light order");
            std::vector<std::uint32_t> indices{123};std::vector<std::array<std::uint32_t,2>> headers{{123,456}};
            reused.gpuIndexData(indices);reused.gpuHeaderData(headers);
            if(indices!=fresh.gpuIndexData()||headers!=fresh.gpuHeaderData())
                throw std::runtime_error("Caller-owned packing storage must replace prior values with the exact GPU data");
            const auto before=reused.gpuHeaderData();auto invalid=desc;invalid.nearPlane=0;
            bool rejected=false;try{reused.configure(invalid);}catch(const std::invalid_argument&){rejected=true;}
            if(!rejected||reused.gpuHeaderData()!=before)
                throw std::runtime_error("Invalid reconfiguration must preserve prior memberships");
        }
    }
    {
        azurerender::ClusteredLightGrid bounded({16,9,24,.1F,100.F});
        azurerender::RenderLight light;light.viewSpaceBounds=true;
        light.clusterPosition[2]=10;light.viewPosition={0,0,10};light.radius=20;
        light.shape.receivers.surfaces=true;light.shape.receivers.characters=false;
        light.shape.influenceBox.enabled=true;
        light.shape.influenceBox.worldToBox={{{1,0,0,0},{0,1,0,0},{0,0,1,1000}}};
        const std::array<float,16> identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
        light.clusterBox=azurerender::clusterInfluenceBox(light.shape.influenceBox,identity);
        bounded.assign({light});
        if(!bounded.gpuIndexData().empty())
            throw std::runtime_error("A separated surface influence box must remove zero-contribution cluster references");
    }
    {
        const std::array<float,16> identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
        auto bad=identity;bad[0]=0;
        azurerender::LightInfluenceBox box;box.enabled=true;
        if(azurerender::clusterInfluenceBox(box,bad))throw std::runtime_error("Singular view must retain radius fallback");
        bad=identity;bad[12]=std::numeric_limits<float>::infinity();
        if(azurerender::clusterInfluenceBox(box,bad))throw std::runtime_error("Nonfinite view must retain radius fallback");
        box.enabled=false;if(azurerender::clusterInfluenceBox(box,identity))throw std::runtime_error("Disabled box must retain radius fallback");
        std::size_t checked=0,removed=0;
        for(bool ortho:{false,true})for(float sign:{-1.F,1.F})for(unsigned pose=0;pose<4;++pose){
            azurerender::ClusterGridDesc desc{16,9,24,.1F,100.F,sign*(ortho?.25F:1.5F),ortho?-.25F:-2.F,ortho,.125F,-.25F};
            auto view=identity;
            if(pose==1){view={.8F,0,-.6F,0,0,1,0,0,.6F,0,.8F,0,2,-1,-.5F,1};}
            if(pose==2){view[12]=-.75F;view[13]=.5F;view[14]=7.95F;}
            if(pose==3)view={1.2F,.1F,0,0,.2F,.8F,.15F,0,.05F,0,1.1F,0,2,3,.5F,1};
            azurerender::RenderLight light;light.radius=1000;light.viewSpaceBounds=true;
            light.shape.receivers.surfaces=true;light.shape.receivers.characters=false;
            light.shape.influenceBox.enabled=true;light.shape.influenceBox.falloffThreshold=1;
            // Rotated, anisotropic world box centred on (0,0,-8).
            light.shape.influenceBox.worldToBox={{{.4F,0,.3F,2.4F},{0,.5F,0,0},{-.15F,0,.2F,1.6F}}};
            const auto projectView=[&](const std::array<float,3>& w){std::array<float,3> v{};
                for(unsigned a=0;a<3;++a)v[a]=view[a]*w[0]+view[4+a]*w[1]+view[8+a]*w[2]+view[12+a];v[2]=-v[2];return v;};
            const auto centre=projectView({0,0,-8});light.viewPosition=centre;light.clusterPosition[2]=centre[2];
            auto global=light;global.shape.type=azurerender::LightKind::Directional;
            auto character=light;character.shape.receivers.characters=true;
            light.clusterBox=azurerender::clusterInfluenceBox(light.shape.influenceBox,view);
            if(!light.clusterBox)throw std::runtime_error("Finite affine view must produce box planes");
            character.clusterBox=light.clusterBox;
            azurerender::ClusteredLightGrid tight(desc),reference(desc);
            tight.assign({global,light,character});auto original=light;original.clusterBox.reset();reference.assign({global,original,character});
            removed+=reference.gpuIndexData().size()-tight.gpuIndexData().size();
            for(unsigned z=0;z<24;++z)for(unsigned y=0;y<9;++y)for(unsigned x=0;x<16;++x){
                const auto& before=reference.indicesFor(x,y,z);const auto& after=tight.indicesFor(x,y,z);
                std::vector<std::uint32_t> expected;for(auto id:before)if(id!=1 || std::find(after.begin(),after.end(),1)!=after.end())expected.push_back(id);
                if(expected!=after)throw std::runtime_error("Box culling must preserve order, directional and character memberships");
            }
            for(int i=-12;i<=12;++i)for(int j=-12;j<=12;++j)for(int k=-12;k<=12;++k){
                const float a=i/12.01F,b=j/12.01F,c=k/12.01F;
                const std::array<float,3> w{1.6F*a-2.4F*c,2*b,-8+1.2F*a+3.2F*c};
                if(light.shape.influenceBox.weight(w)<=0)continue;
                const auto v=projectView(w);if(v[2]<desc.nearPlane||v[2]>desc.farPlane)continue;
                const float px=v[0]*desc.projectionScaleX/(ortho?1:v[2])+desc.projectionOffsetX;
                const float py=v[1]*desc.projectionScaleY/(ortho?1:v[2])+desc.projectionOffsetY;
                if(px< -1||px>1||py< -1||py>1)continue;
                const auto cell=[](float n,unsigned count){return std::min(count-1,unsigned(std::max(0.F,(n+1)*.5F*count)));};
                const auto z=std::min(23U,unsigned(std::clamp(std::log(v[2]/desc.nearPlane)/std::log(desc.farPlane/desc.nearPlane),0.F,.999999F)*24));
                const auto& indices=tight.indicesFor(cell(px,16),cell(py,9),z);
                if(std::find(indices.begin(),indices.end(),1)==indices.end())throw std::runtime_error("Nonzero influence sample lost its cluster light");
                ++checked;
            }
        }
        if(checked<20000||removed<1000)throw std::runtime_error("Conservative box fixture lacks coverage or culling");
        std::cout<<"Nonzero box samples retained: "<<checked<<", separated references removed: "<<removed<<'\n';
    }
    {
        azurerender::ClusteredLightGrid reused({8,4,12,.1F,100.F});
        azurerender::RenderLight l;l.viewSpaceBounds=true;l.viewPosition={0,0,5};l.clusterPosition[2]=5;l.radius=2;
        l.shape.receivers.surfaces=true;l.shape.receivers.characters=false;l.shape.influenceBox.enabled=true;
        const std::array<float,16> view{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
        l.shape.influenceBox.worldToBox={{{1,0,0,0},{0,1,0,0},{0,0,1,5}}};
        l.clusterBox=azurerender::clusterInfluenceBox(l.shape.influenceBox,view);
        for(unsigned step=0;step<24;++step){
            if(step==1)l.radius=4;
            if(step==2)l.clusterPosition[2]=2;
            if(step==3)l.viewPosition[0]=3;
            if(step==4)l.viewSpaceBounds=false;
            if(step==5)l.clusterPosition[0]=-.8F;
            if(step==6)l.shape.receivers.characters=true;
            if(step==7)l.shape.type=azurerender::LightKind::Directional;
            if(step==8)l.shape.type=azurerender::LightKind::Spot;
            if(step==9)l.shape.influenceBox.enabled=false;
            if(step==10)l.clusterBox.reset();
            if(step==11){l.shape.characterResponse.present=true;l.shape.characterResponse.schemaVersion=3;l.shape.characterResponse.projectedDistance=true;}
            if(step==12)l.shape.characterResponse.projectedDistance=false;
            if(step==13)l.shape.receivers.surfaces=false;
            if(step==14)l.viewPosition[2]=50;
            if(step==15)l.shape.influenceBox.enabled=true;
            if(step==16)l.clusterBox=azurerender::clusterInfluenceBox(l.shape.influenceBox,view);
            if(step==17)l.clusterBox->rows[0][3]=10;
            if(step==18)l.clusterBox->roundoffScale[0]=10000;
            if(step==19)l.intensity=100;
            if(step==20)l.color[1]=4;
            auto desc=reused.desc();if(step==21){desc.projectionOffsetX=.5F;reused.configure(desc);}
            if(step==22){desc.z=8;reused.configure(desc);}
            azurerender::ClusteredLightGrid fresh(desc);std::vector<azurerender::RenderLight> lights{l};
            if(step==23)lights.push_back(first);
            fresh.assign(lights);reused.assign(lights);reused.assign(lights);
            if(fresh.gpuHeaderData()!=reused.gpuHeaderData()||fresh.gpuIndexData()!=reused.gpuIndexData())
                throw std::runtime_error("Assignment reuse must invalidate every changed membership input");
        }
    }
    return 0;
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
