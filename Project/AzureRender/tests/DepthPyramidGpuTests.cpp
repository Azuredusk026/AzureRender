#include "render/DepthPyramid.hpp"
#include "resources/BinaryFile.hpp"
#include "support/VulkanComputeFixture.hpp"
#include "support/ComputeTexture.hpp"
#include <filesystem>
#include <cstring>
using namespace azurerender;
namespace {
void require(bool v,const char* text){if(!v)throw std::runtime_error(text);}
std::vector<VkBufferImageCopy> regions(const DepthPyramidOutput& out){
    std::vector<VkBufferImageCopy> result;VkDeviceSize offset=0;
    for(unsigned mip=0;mip<out.levels;++mip){VkBufferImageCopy r{};r.bufferOffset=offset;r.imageSubresource={VK_IMAGE_ASPECT_COLOR_BIT,mip,0,1};
        r.imageExtent={std::max(1U,out.extent.width>>mip),std::max(1U,out.extent.height>>mip),1};
        result.push_back(r);offset+=VkDeviceSize(r.imageExtent.width)*r.imageExtent.height*4;}return result;
}
std::array<std::vector<float>,2> capture(test::VulkanComputeFixture& host,DepthPyramidGpu& pyramid,VkImageView source,bool reverse){
    const auto out=pyramid.output(0);const auto copies=regions(out);const auto& last=copies.back();
    const auto bytes=last.bufferOffset+VkDeviceSize(last.imageExtent.width)*last.imageExtent.height*4;
    std::array<rhi::GpuBuffer,2> buffers{};
    try{
        for(auto& buffer:buffers)buffer=host.allocator.createBuffer(bytes,VK_BUFFER_USAGE_TRANSFER_DST_BIT,true);
        host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer native){
            for(unsigned slot=0;slot<2;++slot)pyramid.record(commands,slot,source,(slot==0)^reverse?DepthReduction::Minimum:DepthReduction::Maximum);
            for(unsigned slot=0;slot<2;++slot){const auto output=pyramid.output(slot);
                commands.imageBarrier({output.image,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,out.levels,
                    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT});
                vkCmdCopyImageToBuffer(native,output.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,buffers[slot].buffer,static_cast<unsigned>(copies.size()),copies.data());
                commands.imageBarrier({output.image,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,out.levels,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_ACCESS_TRANSFER_READ_BIT,VK_ACCESS_SHADER_READ_BIT});
                commands.bufferBarrier({buffers[slot].buffer,0,bytes,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT});}
        });
        std::array<std::vector<float>,2> result;
        for(unsigned slot=0;slot<2;++slot){host.allocator.invalidate(buffers[slot],0,bytes);result[slot].resize(static_cast<std::size_t>(bytes/4));
            std::memcpy(result[slot].data(),buffers[slot].mapped,static_cast<std::size_t>(bytes));host.allocator.destroyBuffer(buffers[slot]);}return result;
    }catch(...){for(auto& buffer:buffers)host.allocator.destroyBuffer(buffer);throw;}
}
std::vector<float> reference(std::vector<float> base,unsigned width,unsigned height,unsigned levels,bool maximum){
    std::vector<float> result=base;
    for(unsigned mip=1;mip<levels;++mip){const unsigned w=std::max(1U,width/2),h=std::max(1U,height/2);std::vector<float> next(w*h);
        for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x){float v=base[(y*2)*width+x*2];
            for(unsigned dy=0;dy<2+height%2;++dy)for(unsigned dx=0;dx<2+width%2;++dx){const auto q=base[std::min(y*2+dy,height-1)*width+std::min(x*2+dx,width-1)];v=maximum?std::max(v,q):std::min(v,q);}next[y*w+x]=v;}
        result.insert(result.end(),next.begin(),next.end());base=std::move(next);width=w;height=h;}
    return result;
}
void synthetic(test::VulkanComputeFixture& host,const std::vector<char>& shader){
    for(const auto extent:{VkExtent2D{17,9},VkExtent2D{1,9}}){const unsigned levels=extent.width==17?5:4;
        std::vector<float> values,red;
        for(unsigned y=0;y<extent.height;++y)for(unsigned x=0;x<extent.width;++x){const float v=float(x)+10*float(y)-8;red.push_back(v);values.insert(values.end(),{v,9999,7777,5555});}
        test::ComputeTexture texture(*host.backend,extent.width,extent.height,1,4,VK_FORMAT_R32G32B32A32_SFLOAT,values);
        DepthPyramidGpu pyramid(*host.backend,extent,levels,2,shader);
        for(bool reverse:{false,true}){const auto pair=capture(host,pyramid,texture.view,reverse);
            for(unsigned slot=0;slot<2;++slot)require(pair[slot]==reference(red,extent.width,extent.height,levels,(slot==1)^reverse),"Odd/degenerate depth levels or distinct in-flight reductions differ");}
    }
    std::cout<<"Synthetic: odd axes, one-pixel axis, all levels, two modes, simultaneous slots and reuse exact\n";
}
void nativeFixture(test::VulkanComputeFixture& host,const std::vector<char>& shader,const std::filesystem::path& root){
    const auto bytes=readBinaryFile((root/"texture-16447-mip-0.bin").u8string());
    test::ComputeTexture source(*host.backend,1280,720,1,VK_FORMAT_R32_SFLOAT,bytes);
    DepthPyramidGpu pyramid(*host.backend,{1280,720},7,2,shader);const auto pair=capture(host,pyramid,source.view,false);std::size_t offset=0;
    for(unsigned mip=0;mip<7;++mip){const auto expected=readBinaryFile((root/("texture-16447-mip-"+std::to_string(mip)+".bin")).u8string());
        require(offset+expected.size()<=pair[1].size()*4,"Native depth level outside readback");
        require(std::memcmp(reinterpret_cast<const char*>(pair[1].data())+offset,expected.data(),expected.size())==0,"Native maximum depth pyramid differs bytewise");offset+=expected.size();}
    std::cout<<"Native: base and six generated depth levels byte-exact\n";
}
}
int main(int argc,char** argv)try{
    require(argc==2||argc==3,"Expected shader and optional native fixture root");test::VulkanComputeFixture host;const auto shader=readBinaryFile(argv[1]);
    if(argc==3)nativeFixture(host,shader,std::filesystem::u8path(argv[2]));else synthetic(host,shader);
    require(host.errors==0&&!host.allocator.statistics().liveBuffers&&!host.allocator.statistics().liveImages,"Depth GPU validation or resource retirement failed");
    std::cout<<"PASS: validation=0 resources=0\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
