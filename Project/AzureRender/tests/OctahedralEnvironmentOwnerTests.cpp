#include "render/OctahedralEnvironmentGpu.hpp"
#include "support/OctahedralEnvironmentFixture.hpp"
#include "rhi/NullRhi.hpp"
#include <iostream>
#include <functional>
using namespace azurerender;
void check(bool value,const char* text){if(!value)throw std::runtime_error(text);}
void reject(const std::function<void()>& call){try{call();}catch(const std::exception&){return;}throw std::runtime_error("Invalid octahedral GPU asset accepted");}
int main()try{
    rhi::NullRhi backend;backend.collectImageUploads=true;auto asset=test::octahedralFixture();
    {
        OctahedralEnvironmentGpu texture(backend,asset);
        const auto& image=texture.image();check(image.arrayLayers==2&&image.mipLevels==3&&image.depth==1,"Array layers and levels retained");
        check(backend.imageUploads.size()==1,"One immutable upload");const auto& upload=backend.imageUploads[0];
        check(upload.pixels.size()==asset.texels.size()*4&&std::memcmp(upload.pixels.data(),asset.texels.data(),upload.pixels.size())==0,"Packed HDR upload is exact");
        check(backend.allocator().statistics().liveImages==1&&backend.allocator().statistics().liveBuffers==0,"Only image remains allocated");
        check(texture.parameters()[0]==asset.uvScaleOffset&&texture.parameters()[1]==std::array<float,4>{1,4,4,2},"Sampling parameter ABI");
        texture.writeTexture(VK_NULL_HANDLE,23);
    }
    check(backend.allocator().statistics().liveImages==0&&backend.allocator().statistics().liveBuffers==0,"All image allocations released");
    backend.calls.clear();backend.unavailableSampledFormats.insert(VK_FORMAT_B10G11R11_UFLOAT_PACK32);
    reject([&]{OctahedralEnvironmentGpu texture(backend,asset);});check(backend.calls.empty(),"Unsupported format rejected before allocation");
    backend.unavailableSampledFormats.clear();asset.texels.pop_back();
    reject([&]{OctahedralEnvironmentGpu texture(backend,asset);});check(backend.calls.empty(),"Invalid payload rejected before allocation");
    std::cout<<"PASS: octahedral array ownership, exact payload, ABI and bounded rejection\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
