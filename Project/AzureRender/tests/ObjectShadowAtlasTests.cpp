#include "render/ObjectShadowAtlas.hpp"
#include "render/ShadowTileBounds.hpp"
#include "rhi/NullRhi.hpp"
#include <cstring>
#include <iostream>
#include <stdexcept>
using namespace azurerender;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class F> void rejects(F action){bool bad=false;try{action();}catch(const std::exception&){bad=true;}require(bad,"Invalid shadow resource operation accepted");}
}
int main(){try{
    rhi::NullRhi backend;
    {
        ObjectShadowAtlas atlas(backend);atlas.initialize();
        require(backend.allocator().statistics().liveImages==0,"Empty object shadow owner must defer its depth image");
        rejects([&]{atlas.initialize();});rejects([&]{atlas.beginDescription();});
        rejects([&]{atlas.ensureImage(2048);});
        atlas.ensureImage(4096);const auto image=atlas.image().image;
        require(atlas.image().width==4096&&atlas.image().height==2048,"Object atlas dimensions differ from bounded page layout");
        const auto pass=atlas.beginDescription();require(pass.clearValues.size()==1&&pass.clearValues[0].depthStencil.depth==1,"Object depth atlas must clear to forward far depth");
        atlas.ensureImage(4096);require(atlas.image().image==image&&backend.allocator().statistics().liveImages==1,"An existing object atlas was allocated twice");
    }
    require(backend.allocator().statistics().liveImages==0,"Object atlas image survived owner destruction");
    backend.unavailableSampledFormats.insert(VK_FORMAT_D16_UNORM);
    {ObjectShadowAtlas atlas(backend);atlas.initialize();rejects([&]{atlas.ensureImage(4096);});}
    require(backend.allocator().statistics().liveImages==0,"Unsupported object depth format mutated image ownership");
    {
        ShadowTileBounds bounds(backend);bounds.initialize({0,0,0,0},VK_NULL_HANDLE,VK_NULL_HANDLE,16,2);
        require(bounds.depthBoundsBytes()==32&&bounds.objectRecordsOffset()==32&&bounds.buffer().size==240,"Object records must have an aligned suffix and a bounded allocation");
        std::memset(bounds.buffer().mapped,0xA5,bounds.buffer().size);
        std::vector<ObjectShadowGpuRecord> records(2);records[0].worldToUnitShadow[0]=3;records[1].worldToUnitShadow[0]=7;
        bounds.writeObjectRecords(records);
        const auto* bytes=static_cast<const unsigned char*>(bounds.buffer().mapped);
        for(unsigned i=0;i<32;++i)require(bytes[i]==0xA5,"Object record upload changed depth bounds");
        ObjectShadowGpuHeader header{};std::memcpy(&header,bytes+32,sizeof(header));
        require(header.version==1&&header.count==2&&header.capacity==2,"Object record header lost version, count or capacity");
        require(std::memcmp(bytes+48,records.data(),192)==0,"Adjacent object records overlap or change payload");
        const std::vector<unsigned char> before(bytes,bytes+bounds.buffer().size);records.push_back({});
        rejects([&]{bounds.writeObjectRecords(records);});require(std::memcmp(before.data(),bytes,before.size())==0,"Rejected object upload mutated its buffer");
        bounds.writeObjectRecords({});std::memcpy(&header,bytes+32,sizeof(header));require(header.count==0,"Empty object upload retained stale records");
    }
    {ShadowTileBounds bounds(backend);bounds.initialize({0,0,0,0},VK_NULL_HANDLE,VK_NULL_HANDLE,8,1);require(bounds.objectRecordsOffset()==16,"Odd tile counts require explicit suffix alignment");}
    require(backend.allocator().statistics().liveBuffers==0,"Object record buffer survived owner destruction");
    std::cout<<"Object depth ownership, deferred allocation and bounded record suffix passed\n";
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
