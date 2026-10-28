#include "render/ReflectionSpatialFilter.hpp"
#include "resources/BinaryFile.hpp"
#include "support/VulkanComputeFixture.hpp"
#include "support/ComputeTexture.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <cstring>
#include <iomanip>
using namespace azurerender;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
std::vector<float> floats(const std::filesystem::path& path){
    const auto bytes=readBinaryFile(path.u8string());
    require(!bytes.empty()&&bytes.size()%4==0&&bytes.size()<=256ULL*1024*1024,"Invalid filter fixture payload");
    std::vector<float> values(bytes.size()/4);std::memcpy(values.data(),bytes.data(),bytes.size());
    for(float value:values)require(std::isfinite(value),"Nonfinite fixture input");return values;
}
using Texture=test::ComputeTexture;
void copyOutput(rhi::ICommandRecorder& commands,const ReflectionFilterOutput& target,const rhi::GpuBuffer& buffer){
    commands.imageBarrier({target.image,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,1,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT});
    commands.copyImageToBuffer(target.image,buffer.buffer,target.extent);
    commands.imageBarrier({target.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,1,
        VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_ACCESS_TRANSFER_READ_BIT,VK_ACCESS_SHADER_READ_BIT});
    commands.bufferBarrier({buffer.buffer,0,buffer.size,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT});
}
std::vector<float> capture(test::VulkanComputeFixture& host,ReflectionSpatialFilterGpu& filter,unsigned slot,
    const ReflectionFilterInputViews& inputs,const ReflectionFilterParameters& parameters){
    const auto target=filter.output(slot);auto buffer=host.allocator.createBuffer(std::uint64_t(inputs.width)*inputs.height*16,VK_BUFFER_USAGE_TRANSFER_DST_BIT,true);
    try{
        host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer){
            filter.record(commands,slot,inputs,parameters);
            copyOutput(commands,target,buffer);
        });
        host.allocator.invalidate(buffer,0,buffer.size);std::vector<float> result(static_cast<std::size_t>(buffer.size/4));
        std::memcpy(result.data(),buffer.mapped,static_cast<std::size_t>(buffer.size));host.allocator.destroyBuffer(buffer);
        for(float value:result)require(std::isfinite(value),"Nonfinite filtered output");return result;
    }catch(...){host.allocator.destroyBuffer(buffer);throw;}
}
std::array<std::vector<float>,2> capturePair(test::VulkanComputeFixture& host,ReflectionSpatialFilterGpu& filter,
    const ReflectionFilterInputViews& inputs,const ReflectionFilterParameters& parameters){
    std::array<rhi::GpuBuffer,2> buffers{};
    try{
        for(auto& buffer:buffers)buffer=host.allocator.createBuffer(std::uint64_t(inputs.width)*inputs.height*16,VK_BUFFER_USAGE_TRANSFER_DST_BIT,true);
        host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer){
            auto unfiltered=parameters;unfiltered.depthFalloff=0;
            filter.record(commands,0,inputs,parameters);filter.record(commands,1,inputs,unfiltered);
            for(unsigned i=0;i<2;++i)copyOutput(commands,filter.output(i),buffers[i]);
        });
        std::array<std::vector<float>,2> result;
        for(unsigned i=0;i<2;++i){host.allocator.invalidate(buffers[i],0,buffers[i].size);result[i].resize(static_cast<std::size_t>(buffers[i].size/4));
            std::memcpy(result[i].data(),buffers[i].mapped,static_cast<std::size_t>(buffers[i].size));host.allocator.destroyBuffer(buffers[i]);}
        return result;
    }catch(...){for(auto& buffer:buffers)host.allocator.destroyBuffer(buffer);throw;}
}
std::vector<float> loadLevels(const std::filesystem::path& root,const nlohmann::json& files){
    std::vector<float> result;for(const auto& file:files){auto values=floats(root/std::filesystem::u8path(file.get<std::string>()));
        require(result.size()+values.size()<=128ULL*1024*1024,"Fixture pyramid exceeds its budget");result.insert(result.end(),values.begin(),values.end());}return result;
}
void nativeFixture(test::VulkanComputeFixture& host,const std::vector<char>& shader,const std::filesystem::path& path,const char* output){
    std::ifstream input(path);nlohmann::json document;input>>document;require(document.at("schemaVersion")==1,"Unknown filter fixture version");
    const unsigned width=document.at("width"),height=document.at("height");const auto root=path.parent_path();
    const auto& p=document.at("parameters");ReflectionFilterParameters parameters;
    parameters.maxLod=p.at("maxLod");parameters.sharpLod=p.value("sharpLod",.25F);parameters.depthFalloff=p.value("depthFalloff",.01F);
    parameters.minimumWeight=p.value("minimumWeight",.01F);parameters.reciprocalDepth=p.at("reciprocalDepth");parameters.depthDecode=p.at("depthDecode").get<std::array<float,2>>();
    const auto encoding=p.at("encoding").get<std::string>();require(encoding=="linear"||encoding=="max-component","Unknown fixture encoding");
    parameters.encoding=encoding=="linear"?ReflectionColorEncoding::Linear:ReflectionColorEncoding::MaxComponent;
    const auto depthLevels=static_cast<unsigned>(document.at("depthFiles").size()),colorLevels=static_cast<unsigned>(document.at("colorFiles").size());
    (void)parameters.frame(width,height,depthLevels,colorLevels);
    Texture depth(*host.backend,width,height,depthLevels,1,VK_FORMAT_R32_SFLOAT,loadLevels(root,document.at("depthFiles")));
    Texture surface(*host.backend,width,height,1,2,VK_FORMAT_R32G32_SFLOAT,floats(root/std::filesystem::u8path(document.at("surfaceFile").get<std::string>())));
    std::unique_ptr<Texture> color;
    if(document.value("colorFormat",std::string("rgba32f"))=="r11g11b10"){
        std::vector<char> bytes;for(const auto& file:document.at("colorFiles")){
            const auto part=readBinaryFile((root/std::filesystem::u8path(file.get<std::string>())).u8string());
            require(bytes.size()+part.size()<=512ULL*1024*1024,"Packed fixture exceeds its budget");bytes.insert(bytes.end(),part.begin(),part.end());
        }
        color=std::make_unique<Texture>(*host.backend,width,height,colorLevels,VK_FORMAT_B10G11R11_UFLOAT_PACK32,bytes);
    }else color=std::make_unique<Texture>(*host.backend,width,height,colorLevels,4,VK_FORMAT_R32G32B32A32_SFLOAT,loadLevels(root,document.at("colorFiles")));
    ReflectionSpatialFilterGpu filter(*host.backend,{width,height},2,shader);
    ReflectionFilterInputViews views{depth.view,surface.view,color->view,width,height,depthLevels,colorLevels};
    std::unique_ptr<Texture> confidence;
    if(document.contains("confidenceFiles")){
        if(document.value("confidenceFormat",std::string("r32f"))=="r8"){
            std::vector<char> bytes;for(const auto& file:document.at("confidenceFiles")){
                const auto part=readBinaryFile((root/file.get<std::string>()).u8string());bytes.insert(bytes.end(),part.begin(),part.end());}
            confidence=std::make_unique<Texture>(*host.backend,width,height,static_cast<unsigned>(document.at("confidenceFiles").size()),VK_FORMAT_R8_UNORM,bytes,1);
        }else confidence=std::make_unique<Texture>(*host.backend,width,height,static_cast<unsigned>(document.at("confidenceFiles").size()),1,VK_FORMAT_R32_SFLOAT,loadLevels(root,document.at("confidenceFiles")));
        views.confidence=confidence->view;views.confidenceLevels=static_cast<unsigned>(document.at("confidenceFiles").size());
    }
    const auto result=capture(host,filter,0,views,parameters);const auto repeated=capture(host,filter,1,views,parameters);
    require(result==repeated,"Independent in-flight outputs must match");
    std::ofstream file(std::filesystem::u8path(output),std::ios::binary);file.write(reinterpret_cast<const char*>(result.data()),static_cast<std::streamsize>(result.size()*4));
    require(bool(file),"Cannot write filter output");
    std::cout<<"Native fixture pixels="<<width*height<<" frameParity=exact\n";
}
void synthetic(test::VulkanComputeFixture& host,const std::vector<char>& shader){
    constexpr unsigned width=17,height=9,levels=4;ReflectionSpatialFilterGpu filter(*host.backend,{width,height},2,shader);
    float maximum=0;unsigned captures=0;
    for(unsigned mode=0;mode<4;++mode){
        const bool edge=mode>=2,compressed=mode==1;std::vector<float> depths,colors,surfaces;
        for(unsigned mip=0;mip<levels;++mip){
            const unsigned w=std::max(1U,width>>mip),h=std::max(1U,height>>mip);
            for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x){
                depths.push_back(edge?(x<w/2?2.F:1000.F):10.F);
                const float a=std::ldexp(1.F,int(mip)-4);
                auto rgb=edge?(x<w/2?std::array<float,3>{1,0,0}:std::array<float,3>{0,1,0}):std::array<float,3>{a,a*.5F,a*.25F};
                if(compressed)rgb=encodeReflectionRadiance(rgb);colors.insert(colors.end(),{rgb[0],rgb[1],rgb[2],1});
            }
        }
        for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x)surfaces.insert(surfaces.end(),{edge?1.F:float(x%3)*.5F,x<width/2?.25F:.75F});
        Texture depth(*host.backend,width,height,levels,1,VK_FORMAT_R32_SFLOAT,depths);
        Texture surface(*host.backend,width,height,1,2,VK_FORMAT_R32G32_SFLOAT,surfaces);
        Texture color(*host.backend,width,height,levels,4,VK_FORMAT_R32G32B32A32_SFLOAT,colors);
        ReflectionFilterParameters parameters;parameters.maxLod=3;parameters.depthFalloff=mode==2?0.F:.01F;
        parameters.encoding=compressed?ReflectionColorEncoding::MaxComponent:ReflectionColorEncoding::Linear;
        const ReflectionFilterInputViews views{depth.view,surface.view,color.view,width,height,levels,levels};
        const auto result=capture(host,filter,0,views,parameters),other=capture(host,filter,1,views,parameters);captures+=2;
        require(result==other,"Synthetic frame slots must match");
        for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){
            const auto offset=(y*width+x)*4;require(result[offset+3]==(x<width/2?.25F:.75F),"Confidence transport must remain independent");
            if(!edge){
                const float lod=float(x%3)*1.5F,low=std::floor(lod),high=std::min(3.F,low+1),fraction=lod-low;
                float a=std::exp2(low)/16,b=std::exp2(high)/16;
                if(compressed){a/=1+a;b/=1+b;}
                float expected=a+(b-a)*fraction;if(compressed)expected/=1-expected;
                for(unsigned c=0;c<3;++c){const auto error=std::abs(result[offset+c]-expected/std::exp2(float(c)));maximum=std::max(maximum,error);require(error<2e-5F,"Analytic constant mip and compressed HDR result");}
            }
        }
        if(edge){const auto green=result[(4*width+7)*4+1];require(mode==2?green>.1F:green<.002F,"Depth guide must suppress the separated green surface");}
        if(mode==3){const auto pair=capturePair(host,filter,views,parameters);captures+=2;
            require(pair[0][(4*width+7)*4+1]<.002F&&pair[1][(4*width+7)*4+1]>.1F,"One submission must retain two independent parameter and output slots");}
    }
    std::cout<<std::setprecision(9)<<"Synthetic captures="<<captures<<" analyticMaximumError="<<maximum<<" frameParity=exact\n";
}
void confidenceFixture(test::VulkanComputeFixture& host,const std::vector<char>& shader){
    constexpr unsigned width=17,height=9,levels=4;
    std::vector<float> depths,colors,masks,surface(width*height*4,0);
    for(unsigned mip=0;mip<levels;++mip){const unsigned count=std::max(1U,width>>mip)*std::max(1U,height>>mip);
        depths.insert(depths.end(),count,1);colors.insert(colors.end(),count*4,.25F);masks.insert(masks.end(),count,float(mip)/7);}
    for(unsigned i=0;i<width*height;++i){surface[i*4]=.75F;surface[i*4+2]=std::array<float,6>{0,.32F,.34F,.66F,.67F,1}[i%6];}
    Texture depth(*host.backend,width,height,levels,1,VK_FORMAT_R32_SFLOAT,depths);
    Texture color(*host.backend,width,height,levels,4,VK_FORMAT_R32G32B32A32_SFLOAT,colors);
    Texture mask(*host.backend,width,height,levels,1,VK_FORMAT_R32_SFLOAT,masks);
    Texture material(*host.backend,width,height,1,4,VK_FORMAT_R32G32B32A32_SFLOAT,surface);
    ReflectionSpatialFilterGpu filter(*host.backend,{width,height},2,shader);
    ReflectionFilterInputViews inputs{depth.view,material.view,color.view,width,height,levels,levels};
    inputs.confidence=mask.view;inputs.confidenceLevels=levels;inputs.roughnessChannel=2;
    ReflectionFilterParameters p;p.maxLod=3;p.encoding=ReflectionColorEncoding::Linear;
    const auto result=capture(host,filter,0,inputs,p);
    for(unsigned i=0;i<width*height;++i)require(std::abs(result[i*4+3]-std::round(std::floor(surface[i*4+2]*3)/7*255)/255)<1e-6F,"Confidence follows floored roughness mip and selected channel");
    inputs.quantizeConfidence=false;const auto wide=capture(host,filter,1,inputs,p);
    for(unsigned i=0;i<width*height;++i)require(std::abs(wide[i*4+3]-std::floor(surface[i*4+2]*3)/7)<1e-6F,"Floating confidence must preserve unquantized values");
    require(std::abs(wide[3*4+3]-result[3*4+3])>1e-4F,"Quantization control must affect confidence");
    std::cout<<"Confidence mip selection, quantization and surface channel passed\n";
}

}
int main(int argc,char** argv)try{
    require(argc==2||argc==4,"Expected shader and optional fixture/output paths");
    test::VulkanComputeFixture host;const auto shader=readBinaryFile(argv[1]);
    if(argc==4)nativeFixture(host,shader,std::filesystem::u8path(argv[2]),argv[3]);else {synthetic(host,shader);confidenceFixture(host,shader);}
    require(host.errors==0,"Vulkan validation reported an error");
    require(host.allocator.statistics().liveBuffers==0&&host.allocator.statistics().liveImages==0,"GPU resources must be released");
    std::cout<<"PASS: validation errors=0 liveBuffers=0 liveImages=0\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
