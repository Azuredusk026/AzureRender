#include "render/ClusteredLightGrid.hpp"
#include <iostream>
#include <stdexcept>
using namespace azurerender;
namespace {
template<class Grid> auto receiverData(Grid& grid,std::vector<std::array<std::uint32_t,2>>& h,std::vector<std::uint32_t>& i,int)
    ->decltype(grid.gpuReceiverData(h,i),void()){grid.gpuReceiverData(h,i);}
template<class Grid> void receiverData(Grid& grid,std::vector<std::array<std::uint32_t,2>>& h,std::vector<std::uint32_t>& i,long){grid.gpuHeaderData(h);grid.gpuIndexData(i);}
void check(ClusteredLightGrid& grid,const std::vector<RenderLight>& lights){
    grid.assign(lights);std::vector<std::array<std::uint32_t,2>> h;std::vector<std::uint32_t> indices;
    receiverData(grid,h,indices,0);
    if(h.size()!=grid.clusterCount()*4)throw std::runtime_error("Every cluster must expose four ordered receiver ranges");
    const auto& d=grid.desc();
    for(unsigned z=0;z<d.z;++z)for(unsigned y=0;y<d.y;++y)for(unsigned x=0;x<d.x;++x){
        const auto cell=(z*d.y+y)*d.x+x;const auto& all=grid.indicesFor(x,y,z);
        for(unsigned category=0;category<4;++category){
            std::vector<std::uint32_t> expected;
            for(auto index:all){const auto& s=lights[index].shape;
                const bool blend=s.characterResponse.isColorBlend();
                const bool include=category==0?s.receivers.surfaces:category==1?s.receivers.characters
                    :category==2?(blend?s.receivers.characters:s.receivers.surfaces):blend&&s.receivers.characters;
                if(include)expected.push_back(index);
            }
            const auto range=h[cell*4+category];if(std::size_t(range[0])+range[1]>indices.size())throw std::runtime_error("Receiver range outside live index prefix");
            const std::vector<std::uint32_t> actual(indices.begin()+range[0],indices.begin()+range[0]+range[1]);
            if(actual!=expected)throw std::runtime_error("Receiver filtering must exactly preserve original light order and blend override");
        }
    }
}
}
int main()try{
    ClusteredLightGrid grid({4,3,8,.1F,100.F});std::vector<RenderLight> lights;
    for(unsigned blend=0;blend<2;++blend)for(unsigned mask=0;mask<4;++mask){
        RenderLight l;l.radius=20;l.viewSpaceBounds=true;l.viewPosition={0,0,10};l.clusterPosition[2]=10;
        l.shape.receivers.surfaces=(mask&1)!=0;l.shape.receivers.characters=(mask&2)!=0;
        if(blend){l.shape.characterResponse.present=true;l.shape.characterResponse.schemaVersion=3;l.shape.characterResponse.projectedDistance=true;}
        lights.push_back(l);
    }
    check(grid,lights);check(grid,lights);
    for(unsigned step=0;step<10;++step){
        if(step==0)lights[0].shape.receivers.surfaces=true;
        if(step==1)lights[1].shape.receivers.characters=true;
        if(step==2)lights[2].shape.characterResponse.present=true;
        if(step==3)lights[2].shape.characterResponse.schemaVersion=3;
        if(step==4)lights[2].shape.characterResponse.projectedDistance=true;
        if(step==5)lights[2].shape.characterResponse.projectedDistance=false;
        if(step==6)std::reverse(lights.begin(),lights.end());
        if(step==7)lights.pop_back();
        if(step==8){auto d=grid.desc();d.x=8;grid.configure(d);}
        if(step==9)lights.clear();
        check(grid,lights);check(grid,lights);
    }
    std::cout<<"Four receiver ranges preserve all masks, blend overrides, order and invalidations\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
