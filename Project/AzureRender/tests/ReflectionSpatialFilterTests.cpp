#include "render/ReflectionSpatialFilter.hpp"
#include "rhi/NullRhi.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
using namespace azurerender;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class F>void rejects(F operation){bool rejected=false;try{operation();}catch(const std::exception&){rejected=true;}require(rejected,"Invalid reflection filter input must fail");}
int main()try{
    ReflectionFilterParameters parameters;parameters.maxLod=3;
    const auto frame=parameters.frame(17,9,4,4);
    static_assert(sizeof(frame)==48,"Reflection filter frame is three vec4 rows");
    require(frame[0][0]==1.F/17&&frame[0][1]==1.F/9&&frame[0][2]==3,"Frame uses actual dimensions and mip range");
    require(frame[2][2]==17&&frame[2][3]==9,"Frame preserves integer output extent");
    for(unsigned width:{0U,8193U})rejects([&]{(void)parameters.frame(width,9,4,4);});
    rejects([&]{(void)parameters.frame(17,9,3,4);});
    rejects([&]{(void)parameters.frame(17,9,4,6);});
    auto invalid=parameters;invalid.depthFalloff=-1;rejects([&]{(void)invalid.frame(17,9,4,4);});
    invalid=parameters;invalid.minimumWeight=0;rejects([&]{(void)invalid.frame(17,9,4,4);});
    invalid=parameters;invalid.maxLod=std::numeric_limits<float>::infinity();rejects([&]{(void)invalid.frame(17,9,4,4);});
    invalid=parameters;invalid.encoding=static_cast<ReflectionColorEncoding>(99);rejects([&]{(void)invalid.frame(17,9,4,4);});
    invalid=parameters;invalid.reciprocalDepth=true;invalid.depthDecode={-1,0};rejects([&]{(void)invalid.frame(17,9,4,4);});
    auto captured=parameters;captured.reciprocalDepth=true;captured.depthDecode={9.999899864196777F,.00009999999747378752F};
    const auto nativeFrame=captured.frame(1280,720,7,7);require(nativeFrame[1][2]==1,"Reciprocal depth is explicit");
    const std::array<float,3> color{2,1,.5F};const auto encoded=encodeReflectionRadiance(color);
    require(std::abs(encoded[0]-2.F/3)<1e-6F&&std::abs(encoded[2]-1.F/6)<1e-6F,"HDR compression preserves channel ratios");
    const auto decoded=decodeReflectionRadiance(encoded);
    for(unsigned i=0;i<3;++i)require(std::abs(decoded[i]-color[i])<2e-6F,"Compressed HDR radiance roundtrip");
    rejects([&]{(void)encodeReflectionRadiance({-1,0,0});});
    rejects([&]{(void)decodeReflectionRadiance({1,0,0});});
    rejects([&]{(void)encodeReflectionRadiance({0,std::numeric_limits<float>::quiet_NaN(),0});});
    rejects([&]{(void)encodeReflectionRadiance({std::numeric_limits<float>::max(),0,0});});
    rhi::NullRhi backend;std::vector<char> code(20);code[0]=3;code[1]=2;code[2]=35;code[3]=7;
    rejects([&]{ReflectionSpatialFilterGpu invalidFilter(backend,{17,9},0,code);});
    rejects([&]{ReflectionSpatialFilterGpu invalidFilter(backend,{8192,8192},2,code);});
    backend.unavailableSampledFormats.insert(VK_FORMAT_R32G32B32A32_SFLOAT);
    rejects([&]{ReflectionSpatialFilterGpu invalidFilter(backend,{17,9},2,code);});
    backend.unavailableSampledFormats.clear();
    require(backend.allocator().statistics().liveBuffers==0&&backend.allocator().statistics().liveImages==0,"Rejected construction allocates no image or buffer");
    {
        ReflectionSpatialFilterGpu filter(backend,{17,9},2,code);
        auto depth=backend.allocator().createImage2D(17,9,VK_FORMAT_R32_SFLOAT,VK_IMAGE_USAGE_SAMPLED_BIT,4);
        auto surface=backend.allocator().createImage2D(17,9,VK_FORMAT_R32G32_SFLOAT,VK_IMAGE_USAGE_SAMPLED_BIT);
        auto colorImage=backend.allocator().createImage2D(17,9,VK_FORMAT_R32G32B32A32_SFLOAT,VK_IMAGE_USAGE_SAMPLED_BIT,4);
        const auto depthView=backend.createImageView(depth.image,VK_FORMAT_R32_SFLOAT,VK_IMAGE_ASPECT_COLOR_BIT,4);
        const auto surfaceView=backend.createImageView(surface.image,VK_FORMAT_R32G32_SFLOAT,VK_IMAGE_ASPECT_COLOR_BIT,1);
        const auto colorView=backend.createImageView(colorImage.image,VK_FORMAT_R32G32B32A32_SFLOAT,VK_IMAGE_ASPECT_COLOR_BIT,4);
        ReflectionFilterInputViews inputs{depthView,surfaceView,colorView,17,9,4,4};
        rhi::NullCommandRecorder commands;filter.record(commands,0,inputs,parameters);
        require(filter.output(0).view!=filter.output(1).view,"In-flight outputs are independent");
        const auto writes=backend.descriptorImageWrites.size(),calls=commands.calls.size();
        rejects([&]{filter.record(commands,2,inputs,parameters);});
        auto missing=inputs;missing.color=VK_NULL_HANDLE;rejects([&]{filter.record(commands,1,missing,parameters);});
        auto alias=inputs;alias.color=filter.output(1).view;rejects([&]{filter.record(commands,1,alias,parameters);});
        require(backend.descriptorImageWrites.size()==writes&&commands.calls.size()==calls,"Invalid input leaves descriptors and commands unchanged");
        filter.record(commands,0,inputs,parameters);
        require(std::count_if(commands.calls.begin(),commands.calls.end(),[](const auto& call){return call.name=="dispatch";})==2,"Repeated output can be recorded after its prior submission completes");
        backend.destroyImageView(colorView);backend.destroyImageView(surfaceView);backend.destroyImageView(depthView);
        backend.allocator().destroyImage(colorImage);backend.allocator().destroyImage(surface);backend.allocator().destroyImage(depth);
    }
    require(backend.allocator().statistics().liveBuffers==0&&backend.allocator().statistics().liveImages==0,"Filter releases owned images and parameter buffers");
    std::cout<<"PASS: reflection filter bounds, HDR transport, independent frame ownership and transactional recording\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
