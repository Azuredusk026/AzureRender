#include "render/IrradianceVolume.hpp"
#include <cassert>
#include <cmath>
#include <limits>
#include <iostream>
using namespace azurerender;
int main(int argc,char** argv) {
    IrradianceVolume field;
    field.origin={0,0,0}; field.spacing=2; field.dimensions={2,2,2}; field.cells.resize(8);
    for(unsigned z=0;z<2;++z)for(unsigned y=0;y<2;++y)for(unsigned x=0;x<2;++x){
        auto& c=field.cells[x+2*y+4*z]; c.weight=1;
        c.coefficients={{{float(x)*4,0,0,-1},{0,float(y)*2,0,1},{0,0,float(z)*2,1}}};
    }
    field.validate();
    auto center=field.query({1,1,1});
    assert(center.weight==1);
    auto rgb=center.evaluate({1,0,0});
    assert((rgb==std::array<float,3>{1,1,1})); // Interpolate before clamp: not 1.5.
    auto edge=field.query({2,2,2}).evaluate({0,0,1}); assert(edge[2]==3);
    assert(field.query({2.01F,1,1}).weight==0);
    field.cells[0].weight=0;
    auto missing=field.query({1,1,1});
    assert(missing.weight==.875F && missing.evaluate({0,1,0})[1]==1.875F);
    auto bytes=field.encode(); auto loaded=IrradianceVolume::decode(bytes);
    assert(loaded.query({1,1,1}).weight==missing.weight);
    auto rejects=[](auto callable){try{callable();}catch(const std::invalid_argument&){return true;}return false;};
    auto broken=bytes; broken[0]=std::byte{0}; assert(rejects([&]{(void)IrradianceVolume::decode(broken);}));
    broken=bytes;broken.pop_back();assert(rejects([&]{(void)IrradianceVolume::decode(broken);}));
    broken=bytes;broken.push_back(std::byte{0});assert(rejects([&]{(void)IrradianceVolume::decode(broken);}));
    broken=bytes;broken[88]=std::byte{1};assert(rejects([&]{(void)IrradianceVolume::decode(broken);}));
    broken=bytes;for(unsigned i=24;i<36;++i)broken[i]=std::byte{255};
    assert(rejects([&]{(void)IrradianceVolume::decode(broken);}));
    assert(rejects([&]{(void)center.evaluate({2,0,0});}));
    field.spacing=0;assert(rejects([&]{field.validate();}));field.spacing=2;
    field.cells[0].weight=2;assert(rejects([&]{field.validate();}));field.cells[0].weight=0;
    field.cells[0].coefficients[0][0]=std::numeric_limits<float>::infinity();
    assert(rejects([&]{field.validate();}));
    if(argc==2){
        const auto native=IrradianceVolume::load(argv[1]);
        const auto contribution=native.query({0,2,8});
        const auto color=contribution.evaluate({0,1,0});
        assert(contribution.weight>0 && color[0]>0 && color[1]>0 && color[2]>0);
        std::cout<<"Loaded "<<native.cells.size()<<" samples, weight="<<contribution.weight
            <<", RGB="<<color[0]<<","<<color[1]<<","<<color[2]<<"\n";
    }
}
