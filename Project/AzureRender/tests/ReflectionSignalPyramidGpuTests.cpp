#include "render/ReflectionSignalPyramid.hpp"
#include "resources/BinaryFile.hpp"
#include "support/VulkanComputeFixture.hpp"
#include "support/ComputeTexture.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <cstring>
using namespace azurerender;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct Capture {
    std::vector<char> color,confidence;
    std::vector<VkBufferImageCopy> colorRegions,confidenceRegions;
};
std::vector<VkBufferImageCopy> regions(unsigned width,unsigned height,unsigned levels,unsigned bytes){
    std::vector<VkBufferImageCopy> result;VkDeviceSize offset=0;
    for(unsigned mip=0;mip<levels;++mip){VkBufferImageCopy region{};region.bufferOffset=(offset+3)&~VkDeviceSize(3);
        region.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,mip,0,1};region.imageExtent={std::max(1U,width>>mip),std::max(1U,height>>mip),1};
        offset=region.bufferOffset+VkDeviceSize(region.imageExtent.width)*region.imageExtent.height*bytes;result.push_back(region);}
    return result;
}
VkDeviceSize storage(const std::vector<VkBufferImageCopy>& layout,unsigned bytes){const auto& last=layout.back();return last.bufferOffset+VkDeviceSize(last.imageExtent.width)*last.imageExtent.height*bytes;}
Capture capture(test::VulkanComputeFixture& host,ReflectionSignalPyramidGpu& pyramid,unsigned slot,VkImageView depth,unsigned depthLevels,
    const ReflectionPyramidParameters& parameters,const std::vector<char>& color,const std::vector<char>& confidence){
    const auto target=pyramid.output(slot);const unsigned colorBytes=pyramid.usesPackedFormats()?4:16,confidenceBytes=pyramid.usesPackedFormats()?1:4;
    require(color.size()==std::size_t(target.width)*target.height*colorBytes&&confidence.size()==std::size_t(target.width)*target.height*confidenceBytes,"Base fixture extent mismatch");
    Capture result;result.colorRegions=regions(target.width,target.height,target.levels,colorBytes);result.confidenceRegions=regions(target.width,target.height,target.levels,confidenceBytes);
    std::array<rhi::GpuBuffer,4> buffers{};
    try{
        const auto usage=VK_BUFFER_USAGE_TRANSFER_SRC_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        buffers[0]=host.allocator.createBuffer(color.size(),usage,true);buffers[1]=host.allocator.createBuffer(confidence.size(),usage,true);
        buffers[2]=host.allocator.createBuffer(storage(result.colorRegions,colorBytes),usage,true);buffers[3]=host.allocator.createBuffer(storage(result.confidenceRegions,confidenceBytes),usage,true);
        std::memcpy(buffers[0].mapped,color.data(),color.size());std::memcpy(buffers[1].mapped,confidence.data(),confidence.size());
        for(unsigned i=0;i<2;++i)host.allocator.flush(buffers[i],0,buffers[i].size);
        host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer native){
            pyramid.beginBaseWrite(commands,slot);
            VkBufferImageCopy base{};base.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};base.imageExtent={target.width,target.height,1};
            vkCmdCopyBufferToImage(native,buffers[0].buffer,target.colorImage,VK_IMAGE_LAYOUT_GENERAL,1,&base);
            vkCmdCopyBufferToImage(native,buffers[1].buffer,target.confidenceImage,VK_IMAGE_LAYOUT_GENERAL,1,&base);
            pyramid.record(commands,slot,depth,depthLevels,parameters);
            for(unsigned i=0;i<2;++i){const auto image=i?target.confidenceImage:target.colorImage;const auto& layout=i?result.confidenceRegions:result.colorRegions;
                commands.imageBarrier({image,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,target.levels,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT});
                vkCmdCopyImageToBuffer(native,image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,buffers[i+2].buffer,static_cast<unsigned>(layout.size()),layout.data());
                commands.imageBarrier({image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,target.levels,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_ACCESS_TRANSFER_READ_BIT,VK_ACCESS_SHADER_READ_BIT});
                commands.bufferBarrier({buffers[i+2].buffer,0,buffers[i+2].size,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT});
            }
        });
        for(unsigned i=0;i<2;++i){auto& out=i?result.confidence:result.color;auto& buffer=buffers[i+2];host.allocator.invalidate(buffer,0,buffer.size);
            out.resize(static_cast<std::size_t>(buffer.size));std::memcpy(out.data(),buffer.mapped,out.size());}
        for(auto& buffer:buffers)host.allocator.destroyBuffer(buffer);return result;
    }catch(...){for(auto& buffer:buffers)host.allocator.destroyBuffer(buffer);throw;}
}
template<class T>std::vector<char> bytes(const std::vector<T>& values){std::vector<char> result(values.size()*sizeof(T));std::memcpy(result.data(),values.data(),result.size());return result;}
float scalar(const std::vector<char>& values,std::size_t offset){float result;std::memcpy(&result,values.data()+offset,4);return result;}
void synthetic(test::VulkanComputeFixture& host,const ReflectionPyramidShaders& shaders){
    constexpr unsigned width=17,height=9,levels=4;std::vector<float> depthValues;
    for(unsigned mip=0;mip<levels;++mip)depthValues.insert(depthValues.end(),std::max(1U,width>>mip)*std::max(1U,height>>mip),1.F);
    test::ComputeTexture depth(*host.backend,width,height,levels,1,VK_FORMAT_R32_SFLOAT,depthValues);
    for(auto precision:{ReflectionPyramidPrecision::RequirePacked,ReflectionPyramidPrecision::Float32}){
        ReflectionSignalPyramidGpu pyramid(*host.backend,{width,height},levels,2,precision,shaders);
        std::vector<char> color,confidence;const bool packed=pyramid.usesPackedFormats();
        if(packed){color=bytes(std::vector<std::uint32_t>(width*height,0x781e03c0U));confidence=bytes(std::vector<std::uint8_t>(width*height,255));}
        else {std::vector<float> values(width*height*4,1);color=bytes(values);confidence=bytes(std::vector<float>(width*height,1));}
        const auto first=capture(host,pyramid,0,depth.view,levels,{},color,confidence);
        const auto second=capture(host,pyramid,1,depth.view,levels,{},color,confidence);
        const auto repeated=capture(host,pyramid,0,depth.view,levels,{},color,confidence);
        require(first.color==second.color&&first.confidence==second.confidence&&first.color==repeated.color&&first.confidence==repeated.confidence,"Slot isolation and fenced reuse must preserve every mip");
        const auto offset=first.confidenceRegions[1].bufferOffset;
        if(packed){require(static_cast<unsigned char>(first.confidence[offset])==195,"Packed edge must include zero UAV border energy");require(static_cast<unsigned char>(first.confidence[offset+9])==255,"Packed interior constant must remain one");}
        else {require(std::abs(scalar(first.confidence,offset)-49.F/64)<1e-6F,"Wide edge must include zero UAV border energy");require(std::abs(scalar(first.confidence,offset+9*4)-1)<1e-6F,"Wide interior constant must remain one");}
    }
    std::cout<<"Synthetic: packed/wide odd extent, all mips, zero borders, two slots and fenced reuse passed\n";
}
void nativeFixture(test::VulkanComputeFixture& host,const ReflectionPyramidShaders& shaders,const std::filesystem::path& fixture,const std::filesystem::path& output){
    nlohmann::json doc;std::ifstream(fixture)>>doc;require(doc.at("schemaVersion")==1,"Unknown pyramid fixture version");const auto root=fixture.parent_path();
    const unsigned width=doc.at("width"),height=doc.at("height"),levels=doc.at("levels");require(levels<=14&&width<=8192&&height<=8192,"Fixture exceeds bounded dimensions");
    std::vector<char> depthBytes;for(const auto& name:doc.at("depthFiles")){const auto part=readBinaryFile((root/name.get<std::string>()).u8string());depthBytes.insert(depthBytes.end(),part.begin(),part.end());}
    test::ComputeTexture depth(*host.backend,width,height,levels,VK_FORMAT_R32_SFLOAT,depthBytes);
    ReflectionSignalPyramidGpu pyramid(*host.backend,{width,height},levels,2,ReflectionPyramidPrecision::RequirePacked,shaders);
    ReflectionPyramidParameters p;p.reciprocalDepth=true;p.depthDecode=doc.at("depthDecode").get<std::array<float,2>>();
    const auto result=capture(host,pyramid,0,depth.view,levels,p,readBinaryFile((root/doc.at("colorFile").get<std::string>()).u8string()),readBinaryFile((root/doc.at("confidenceFile").get<std::string>()).u8string()));
    std::filesystem::create_directories(output);
    for(unsigned mip=0;mip<levels;++mip)for(unsigned channel=0;channel<2;++channel){const auto& data=channel?result.confidence:result.color;const auto& region=(channel?result.confidenceRegions:result.colorRegions)[mip];
        const auto size=std::size_t(region.imageExtent.width)*region.imageExtent.height*(channel?1:4);std::ofstream stream(output/((channel?"confidence-":"color-")+std::to_string(mip)+".bin"),std::ios::binary);
        stream.write(data.data()+region.bufferOffset,static_cast<std::streamsize>(size));require(bool(stream),"Cannot save mip output");}
    std::cout<<"Native pyramid levels="<<levels<<" pixels="<<width*height<<'\n';
}
}
int main(int argc,char** argv)try{
    require(argc==3||argc==5,"Expected packed/wide shaders and optional fixture/output");test::VulkanComputeFixture host;
    ReflectionPyramidShaders shaders{readBinaryFile(argv[1]),readBinaryFile(argv[2])};
    if(argc==5)nativeFixture(host,shaders,std::filesystem::u8path(argv[3]),std::filesystem::u8path(argv[4]));else synthetic(host,shaders);
    require(host.errors==0,"Vulkan validation reported errors");require(host.allocator.statistics().liveBuffers==0&&host.allocator.statistics().liveImages==0,"GPU resource leak");
    std::cout<<"PASS: validation errors=0 liveBuffers=0 liveImages=0\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
