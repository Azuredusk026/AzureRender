#include "render/ReflectionTemporal.hpp"
#include "resources/BinaryFile.hpp"
#include "support/VulkanComputeFixture.hpp"
#include "support/ComputeTexture.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <cstring>
using namespace azurerender;
namespace {
void require(bool v,const char* text){if(!v)throw std::runtime_error(text);}
struct Output {std::vector<char> color,confidence;bool packed=false;};
std::array<Output,2> capture(test::VulkanComputeFixture& host,ReflectionTemporalGpu& temporal,ReflectionSignalPyramidGpu& signals,
    const ReflectionTemporalInputs& input,const ReflectionTemporalParameters& parameters){
    const auto base=signals.base(0);const bool packed=signals.usesPackedFormats();const VkDeviceSize pixels=VkDeviceSize(base.width)*base.height;
    std::array<rhi::GpuBuffer,4> buffers{};
    try{
        for(unsigned slot=0;slot<2;++slot){buffers[slot*2]=host.allocator.createBuffer(pixels*(packed?4:16),VK_BUFFER_USAGE_TRANSFER_DST_BIT,true);
            buffers[slot*2+1]=host.allocator.createBuffer(pixels*(packed?1:4),VK_BUFFER_USAGE_TRANSFER_DST_BIT,true);}
        host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer){
            for(unsigned slot=0;slot<2;++slot){signals.beginBaseWrite(commands,slot);auto settings=parameters;
                if(slot)settings.historyWeight=0;
                temporal.record(commands,slot,input,signals.base(slot),settings);
                signals.record(commands,slot,input.weight,1,{});}
            for(unsigned slot=0;slot<2;++slot){const auto output=signals.base(slot);
                const VkImage images[]={output.colorImage,output.confidenceImage};
                for(unsigned index=0;index<2;++index){auto& buffer=buffers[slot*2+index];
                    commands.imageBarrier({images[index],VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,1,
                        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT});
                    commands.copyImageToBuffer(images[index],buffer.buffer,{base.width,base.height});
                    commands.imageBarrier({images[index],VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,1,
                        VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_ACCESS_TRANSFER_READ_BIT,VK_ACCESS_SHADER_READ_BIT});
                    commands.bufferBarrier({buffer.buffer,0,buffer.size,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT});}
            }
        });
        std::array<Output,2> result;
        for(unsigned slot=0;slot<2;++slot){auto& output=result[slot];output.packed=packed;
            for(unsigned index=0;index<2;++index){auto& buffer=buffers[slot*2+index];host.allocator.invalidate(buffer,0,buffer.size);
                auto& bytes=index?output.confidence:output.color;bytes.resize(static_cast<std::size_t>(buffer.size));std::memcpy(bytes.data(),buffer.mapped,bytes.size());host.allocator.destroyBuffer(buffer);}}
        return result;
    }catch(...){for(auto& buffer:buffers)host.allocator.destroyBuffer(buffer);throw;}
}
float unpack(unsigned value,unsigned mantissa){
    const unsigned fraction=value&((1U<<mantissa)-1),exponent=value>>mantissa;
    return exponent?std::ldexp(1.F+float(fraction)/float(1U<<mantissa),int(exponent)-15):std::ldexp(float(fraction),1-15-int(mantissa));
}
float colorAt(const Output& output,unsigned pixel,unsigned channel){
    if(!output.packed){float v=0;std::memcpy(&v,output.color.data()+(pixel*4+channel)*4,4);return v;}
    std::uint32_t value=0;std::memcpy(&value,output.color.data()+pixel*4,4);
    return unpack((value>>(channel==0?0:channel==1?11:22))&(channel==2?1023U:2047U),channel==2?5:6);
}
float confidenceAt(const Output& output,unsigned pixel){
    if(output.packed)return float(static_cast<unsigned char>(output.confidence[pixel]))/255;
    float value=0;std::memcpy(&value,output.confidence.data()+pixel*4,4);return value;
}
float nativeUnit(float expected,float actual,unsigned channel){
    int exponent=0;std::frexp(actual<expected?std::nextafter(expected,0.F):expected,&exponent);
    return std::ldexp(1.F,exponent-1-(channel==2?5:6));
}
void synthetic(test::VulkanComputeFixture& host,const ReflectionTemporalShaders& shaders,const ReflectionPyramidShaders& pyramidShaders){
    constexpr unsigned width=17,height=9;
    for(bool packed:{false,true}){
        ReflectionSignalPyramidGpu signals(*host.backend,{width,height},1,2,packed?ReflectionPyramidPrecision::RequirePacked:ReflectionPyramidPrecision::Float32,pyramidShaders);
        const auto format=signals.base(0);ReflectionTemporalGpu temporal(*host.backend,{width,height},2,format.colorFormat,format.confidenceFormat,shaders);
        for(unsigned mode=0;mode<6;++mode){std::vector<float> current,previous,mask,previousMask,motion,weight;
            for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){const bool flat=mode==3;const float c=flat?.125F:((x+y)%2?.375F:.125F);
                const float history=flat?1.F:.1875F+.125F*float(x)/float(width-1);
                current.insert(current.end(),{c,c,c,1});previous.insert(previous.end(),{history,history,history,1});
                mask.push_back(flat?.25F:((x+y)%2?.75F:.25F));previousMask.push_back(flat?1.F:.4F+.2F*float(x)/float(width-1));
                weight.push_back(mode==5?0.F:float(x%3)*.5F);}
            for(unsigned i=0;i<width*height*4;++i)motion.insert(motion.end(),{mode==2?2.F:mode==4?1.F/width:0.F,0});
            test::ComputeTexture c(*host.backend,width,height,1,4,VK_FORMAT_R32G32B32A32_SFLOAT,current),p(*host.backend,width,height,1,4,VK_FORMAT_R32G32B32A32_SFLOAT,previous);
            test::ComputeTexture m(*host.backend,width,height,1,1,VK_FORMAT_R32_SFLOAT,mask),pm(*host.backend,width,height,1,1,VK_FORMAT_R32_SFLOAT,previousMask);
            test::ComputeTexture vectors(*host.backend,width*2,height*2,1,2,VK_FORMAT_R32G32_SFLOAT,motion),acceptance(*host.backend,width,height,1,1,VK_FORMAT_R32_SFLOAT,weight);
            ReflectionTemporalInputs inputs{c.view,m.view,p.view,pm.view,vectors.view,acceptance.view,width,height};ReflectionTemporalParameters parameters;parameters.historyValid=mode!=1;
            // Isolate zero-radius clipping from floating-point moment cancellation.
            if(mode==3)parameters.varianceScale=0;
            const auto outputs=capture(host,temporal,signals,inputs,parameters);
            for(unsigned slot=0;slot<2;++slot)for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){const auto i=y*width+x;
                float expected=current[i*4],expectedMask=mask[i];
                const bool accumulated=!slot&&mode!=1&&mode!=2&&mode!=3&&mode!=5&&(mode!=4||x>0);
                if(accumulated){const auto previousIndex=y*width+(mode==4?x-1:x);const float w=.97F*weight[i];
                    expected=std::fma(w,previous[previousIndex*4]-expected,expected);expectedMask=std::fma(w,previousMask[previousIndex]-expectedMask,expectedMask);}
                for(unsigned ch=0;ch<3;++ch){const auto value=colorAt(outputs[slot],i,ch);const bool correct=std::isfinite(value)&&std::abs(value-expected)<(packed?nativeUnit(expected,value,ch)*1.01F:2e-6F);
                    if(!correct)std::cerr<<"packed="<<packed<<" mode="<<mode<<" slot="<<slot<<" x="<<x<<" y="<<y<<" channel="<<ch<<" actual="<<value<<" expected="<<expected<<'\n';
                    require(correct,"Temporal colour, clipping, reprojection or slot isolation differs from analytic input");}
                const auto value=confidenceAt(outputs[slot],i);require(std::isfinite(value)&&std::abs(value-expectedMask)<(packed?1.F/255+1e-6F:2e-6F),"Temporal confidence, invalid history or acceptance control differs");
            }
        }
    }
    std::cout<<"Synthetic: 24 outputs, history/invalid/outside/clipping/reprojection/zero-weight, two formats and slot reuse passed\n";
}
void save(const std::filesystem::path& path,const std::vector<char>& bytes){std::ofstream stream(path,std::ios::binary);stream.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));require(bool(stream),"Cannot save temporal fixture output");}
void nativeFixture(test::VulkanComputeFixture& host,const ReflectionTemporalShaders& shaders,const ReflectionPyramidShaders& pyramidShaders,const std::filesystem::path& root,const std::filesystem::path& output){
    std::ifstream stream(root/"fixture.json");nlohmann::json metadata;stream>>metadata;require(metadata.at("schemaVersion")==1,"Unknown temporal fixture");
    const unsigned width=metadata.at("width"),height=metadata.at("height"),motionWidth=metadata.at("motionWidth"),motionHeight=metadata.at("motionHeight");
    require(!std::filesystem::exists(output),"Temporal outputs must use a new directory");std::filesystem::create_directories(output);
    const auto read=[&](const char* name){return readBinaryFile((root/name).u8string());};
    test::ComputeTexture c(*host.backend,width,height,1,VK_FORMAT_B10G11R11_UFLOAT_PACK32,read("current-color.bin"));
    test::ComputeTexture p(*host.backend,width,height,1,VK_FORMAT_B10G11R11_UFLOAT_PACK32,read("previous-color.bin"));
    test::ComputeTexture m(*host.backend,width,height,1,VK_FORMAT_R8_UNORM,read("current-confidence.bin"),1),pm(*host.backend,width,height,1,VK_FORMAT_R8_UNORM,read("previous-confidence.bin"),1);
    test::ComputeTexture motion(*host.backend,motionWidth,motionHeight,1,VK_FORMAT_R32G32_SFLOAT,read("motion-rg32.bin"),8),weight(*host.backend,width,height,1,VK_FORMAT_R32_SFLOAT,read("weight-r32.bin"));
    for(bool packed:{false,true}){
        ReflectionSignalPyramidGpu signals(*host.backend,{width,height},1,2,packed?ReflectionPyramidPrecision::RequirePacked:ReflectionPyramidPrecision::Float32,pyramidShaders);
        const auto format=signals.base(0);ReflectionTemporalGpu temporal(*host.backend,{width,height},2,format.colorFormat,format.confidenceFormat,shaders);
        ReflectionTemporalParameters parameters;parameters.historyValid=true;parameters.rejectOutside=false;
        const auto result=capture(host,temporal,signals,{c.view,m.view,p.view,pm.view,motion.view,weight.view,width,height},parameters);
        for(unsigned slot=0;slot<2;++slot){const auto prefix=std::string(packed?"packed-":"float-")+std::to_string(slot);
            save(output/(prefix+"-color.bin"),result[slot].color);save(output/(prefix+"-confidence.bin"),result[slot].confidence);}
    }
    std::cout<<"Native temporal fixture: two formats, active and zero-weight slots captured\n";
}
}
int main(int argc,char** argv)try{
    require(argc==5||argc==7,"Expected temporal and pyramid shader pairs and optional fixture/output directories");test::VulkanComputeFixture host;
    const ReflectionTemporalShaders shaders{readBinaryFile(argv[1]),readBinaryFile(argv[2])};const ReflectionPyramidShaders pyramid{readBinaryFile(argv[3]),readBinaryFile(argv[4])};
    if(argc==7)nativeFixture(host,shaders,pyramid,std::filesystem::u8path(argv[5]),std::filesystem::u8path(argv[6]));else synthetic(host,shaders,pyramid);
    require(host.errors==0&&!host.allocator.statistics().liveImages&&!host.allocator.statistics().liveBuffers,"Temporal validation or resource retirement failed");
    std::cout<<"PASS: validation=0 resources=0\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
