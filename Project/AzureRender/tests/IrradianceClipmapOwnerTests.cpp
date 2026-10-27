#include "render/IrradianceClipmapGpu.hpp"
#include "support/IrradianceClipmapFixture.hpp"
#include "rhi/NullRhi.hpp"
#include <iostream>
#include <functional>
using namespace azurerender;
void check(bool p,const char* text){if(!p)throw std::runtime_error(text);}
void reject(const std::function<void()>& f){try{f();}catch(const std::exception&){return;}throw std::runtime_error("Invalid GPU clipmap must be rejected");}
int main()try{
    rhi::NullRhi backend;backend.collectImageUploads=true;
    auto asset=test::clipmapFixture();
    {
        IrradianceClipmapGpu volume(backend,asset);
        check(backend.allocator().statistics().liveImages==6,"GPU owner retains six volumes");
        check(backend.allocator().statistics().liveBuffers==1,"Only immutable parameters remain resident");
        const auto parameters=asset.shaderParameters();
        check(volume.parameters().size==sizeof(parameters)&&std::memcmp(volume.parameters().mapped,parameters.data(),sizeof(parameters))==0,"Parameter ABI");
        check(backend.imageUploads.size()==6,"Every layer and directional block is uploaded");
        for(unsigned i=0;i<6;++i){const auto& upload=backend.imageUploads[i];
            check(upload.depth==2&&upload.height==(i%2?6U:2U),"Three-dimensional dimensions retained");
            const void* raw=i%2?static_cast<const void*>(asset.directional[i/2].data()):static_cast<const void*>(asset.amplitude[i/2].data());
            check(upload.pixels.size()==(i%2?96U:32U)&&std::memcmp(raw,upload.pixels.data(),upload.pixels.size())==0,"Lossless amplitude and all directional RGBA bytes");}
        volume.writeTextures(VK_NULL_HANDLE,32);
    }
    check(backend.allocator().statistics().liveImages==0&&backend.allocator().statistics().liveBuffers==0,"GPU owner releases all allocations");
    backend.calls.clear();backend.unavailableSampledFormats.insert(VK_FORMAT_B10G11R11_UFLOAT_PACK32);
    reject([&]{IrradianceClipmapGpu invalid(backend,asset);});check(backend.calls.empty(),"Unsupported format is rejected before allocations");
    backend.unavailableSampledFormats.clear();asset.dimensions={257,1,1};
    for(unsigned i=0;i<3;++i){asset.amplitude[i].assign(257,0);asset.directional[i].assign(257*12,0);}
    asset.validate();reject([&]{IrradianceClipmapGpu invalid(backend,asset);});
    check(backend.calls.empty()&&backend.allocator().statistics().liveImages==0,"GPU dimension budget is checked before allocation");
    std::cout<<"PASS: complete payloads, parameter ABI, GPU bounds, format rejection and lifetime\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
