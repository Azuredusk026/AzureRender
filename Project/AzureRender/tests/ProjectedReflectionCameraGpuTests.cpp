#include "support/VulkanComputeFixture.hpp"
#include "support/ComputeTexture.hpp"
#include "support/ComputeStorage.hpp"
#include "render/ProjectedReflectionCamera.hpp"
#include "render/ProjectedReflectionPreparation.hpp"
#include "render/LinearMath.hpp"
#include "platform/NativeArguments.hpp"
#include "resources/BinaryFile.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <limits>
using namespace azurerender;
namespace {
using Json=nlohmann::json;
template<class T>std::vector<char> bytes(const std::vector<T>& values){std::vector<char> result(values.size()*sizeof(T));std::memcpy(result.data(),values.data(),result.size());return result;}
struct Buffers {
    rhi::IRhi& backend;std::array<rhi::GpuBuffer,4> values{};
    explicit Buffers(rhi::IRhi& value):backend(value){}
    ~Buffers(){for(auto& value:values)backend.allocator().destroyBuffer(value);}
};
}
int main(int argc,char** argv)try{
    const auto args=nativeArguments(argc,argv);
    if(args.size()!=4&&args.size()!=5)throw std::runtime_error("Expected shader, new output directory, mirror case and optional preparation shader");
    const auto output=std::filesystem::absolute(std::filesystem::u8path(args[2]));
    if(!std::filesystem::create_directory(output))throw std::runtime_error("Expected a new output directory");
    const auto& mode=args[3];if(mode!="positive"&&mode!="wide-edge"&&mode!="history-static"&&mode!="nonfinite-motion"&&mode!="wrong-convention"&&mode!="wrong-motion")throw std::runtime_error("Unknown physical mirror mode");
    constexpr unsigned width=640,height=480,traceWidth=320,traceHeight=240;
    ReflectionTraceView camera;camera.projection=internal::perspective(3.14159265358979323846F/3,float(width)/height,.1F,1000.F);
    camera.worldToClip=camera.projection;camera.right={1,0,0};camera.up={0,1,0};camera.forward={0,0,-1};
    auto view=projectedReflectionView(camera);
    ProjectedReflectionSettings settings;settings.maxSteps=64;settings.motionBlend=1;settings.motionEncoding=ProjectedReflectionMotionEncoding::LinearUv;
    if(mode=="history-static")settings.motionBlend=0;
    if(mode=="wide-edge")settings.edgeFade=.5F;
    if(mode=="wrong-convention")view.convention=ProjectedReflectionConvention::MirroredViewY;
    if(mode=="wrong-motion")settings.motionEncoding=ProjectedReflectionMotionEncoding::FourthRoot;
    const auto surface=[&](float v){
        const float dy=(2*v-1)/camera.projection[5];const float denominator=dy+.08F;const float floorDistance=denominator<0?-30.F/denominator:100000.F;
        return std::array<float,2>{{std::min(180.F,floorDistance),floorDistance<180?1.F:0.F}};
    };
    std::vector<float> scene(width*height*4),motion(width*height*4,0),depth(traceWidth*traceHeight),normal(traceWidth*traceHeight*4),snapshot(width*height*4);
    if(mode=="nonfinite-motion")for(std::size_t index=0;index<motion.size();index+=4){motion[index]=std::numeric_limits<float>::quiet_NaN();motion[index+1]=std::numeric_limits<float>::infinity();}
    std::vector<std::uint8_t> mask(traceWidth*traceHeight,255),eligibility(traceWidth*traceHeight,0);
    for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){const auto value=surface((y+.5F)/height);const auto i=(y*width+x)*4;
        const bool lowerWall=(y+.5F)/height>.5F;
        scene[i]=value[1]>0?0.F:lowerWall?1.F:0.F;scene[i+1]=value[1]>0||lowerWall?0.F:1.F;scene[i+2]=value[1]>0?1.F:0.F;scene[i+3]=1;
        snapshot[i]=value[0];snapshot[i+1]=value[1]>0?.5F+.04F/1.08F:.5F;snapshot[i+2]=value[1]>0?1.F:.5F;snapshot[i+3]=value[1]>0?.15F:1.F;}
    for(unsigned y=0;y<traceHeight;++y)for(unsigned x=0;x<traceWidth;++x){const auto value=surface((y+.5F)/traceHeight);const auto i=y*traceWidth+x;
        depth[i]=(1.F/value[0]-view.depthReciprocal[1])/view.depthReciprocal[0];
        normal[i*4]=.5F;normal[i*4+1]=value[1]>0?.5F-.04F/1.08F:1.F;normal[i*4+2]=value[1]>0?.15F:1.F;
        eligibility[i]=value[1]>0?255:0;}
    std::vector<std::uint32_t> tiles;for(unsigned y=0;y<traceHeight/8;++y)for(unsigned x=0;x<traceWidth/8;++x)tiles.push_back((x<<16)|y);
    test::VulkanComputeFixture host;Json report={{"status","failed"},{"mode",mode},{"device",host.properties.deviceName}};bool correct=false;
    {
        auto& backend=*host.backend;Buffers buffers(backend);const auto tileData=bytes(tiles);
        buffers.values[0]=backend.allocator().createBuffer(tileData.size(),VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,true);
        std::memcpy(buffers.values[0].mapped,tileData.data(),tileData.size());backend.allocator().flush(buffers.values[0],0,tileData.size());
        test::ComputeTexture color(backend,width,height,1,VK_FORMAT_R32G32B32A32_SFLOAT,bytes(scene),16);
        test::ComputeTexture deviceDepth(backend,traceWidth,traceHeight,1,VK_FORMAT_R32_SFLOAT,bytes(depth),4);
        test::ComputeTexture delta(backend,width,height,1,VK_FORMAT_R32G32B32A32_SFLOAT,bytes(motion),16);
        test::ComputeTexture surfaceMask(backend,traceWidth,traceHeight,1,VK_FORMAT_R8_UNORM,bytes(mask),1);
        test::ComputeTexture normalRoughness(backend,traceWidth,traceHeight,1,VK_FORMAT_R32G32B32A32_SFLOAT,bytes(normal),16);
        test::ComputeTexture snapshotSurface(backend,width,height,1,VK_FORMAT_R32G32B32A32_SFLOAT,bytes(snapshot),16);
        test::ComputeStorage outputColor(backend,traceWidth,traceHeight,VK_FORMAT_B10G11R11_UFLOAT_PACK32,4,std::vector<char>(traceWidth*traceHeight*4,0));
        test::ComputeStorage outputFootprint(backend,traceWidth,traceHeight,VK_FORMAT_R16G16_UNORM,4,std::vector<char>(traceWidth*traceHeight*4,0));
        test::ComputeStorage outputConfidence(backend,traceWidth,traceHeight,VK_FORMAT_R8_UNORM,1,bytes(eligibility));
        std::array<VkImage,3> storage{{outputColor.image.image,outputFootprint.image.image,outputConfidence.image.image}};
        for(unsigned i=0;i<3;++i)buffers.values[1+i]=backend.allocator().createBuffer(traceWidth*traceHeight*(i==2?1:4),VK_BUFFER_USAGE_TRANSFER_DST_BIT,true);
        ProjectedReflectionRayGpu kernel(backend,1,readBinaryFile(args[1]));ProjectedReflectionInputs inputs;
        inputs.previousColor=color.view;inputs.previousDepth=inputs.currentDepth=deviceDepth.view;inputs.motion=delta.view;inputs.surfaceMask=surfaceMask.view;inputs.normalRoughness=normalRoughness.view;
        inputs.sourceExtent={width,height};inputs.traceExtent={traceWidth,traceHeight};inputs.tiles=buffers.values[0].buffer;inputs.tileBytes=tileData.size();inputs.tileCount=static_cast<unsigned>(tiles.size());
        ProjectedReflectionOutputs outputs{outputColor.view,outputFootprint.view,outputConfidence.view,{traceWidth,traceHeight}};
        std::unique_ptr<ProjectedReflectionPreparationGpu> preparation;
        if(args.size()==5){
            preparation=std::make_unique<ProjectedReflectionPreparationGpu>(backend,VkExtent2D{width,height},1,readBinaryFile(args[4]),2);
            const auto prepared=preparation->output(0);inputs.currentDepth=inputs.previousDepth=prepared.depth;inputs.surfaceMask=prepared.mask;inputs.normalRoughness=prepared.normalRoughness;
            inputs.tiles=prepared.tiles;inputs.tileBytes=prepared.tileBytes;inputs.tileCount=prepared.tileCount;outputs.footprint=prepared.footprint;storage[1]=prepared.footprintImage;
        }
        report["surfacePreparation"]=preparation!=nullptr;
        host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer){
            if(preparation)preparation->record(commands,0,snapshotSurface.view,{outputConfidence.image.image,outputConfidence.view,{traceWidth,traceHeight}},view.depthReciprocal,{});
            commands.bufferBarrier({inputs.tiles,0,inputs.tileBytes,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_HOST_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT});
            kernel.record(commands,0,inputs,outputs,view,settings);
            for(unsigned i=0;i<3;++i){auto& buffer=buffers.values[1+i];commands.imageBarrier({storage[i],VK_IMAGE_LAYOUT_GENERAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,1,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_ACCESS_SHADER_WRITE_BIT,VK_ACCESS_TRANSFER_READ_BIT});
                commands.copyImageToBuffer(storage[i],buffer.buffer,{traceWidth,traceHeight});
                commands.bufferBarrier({buffer.buffer,0,buffer.size,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT});}
        });
        for(unsigned i=0;i<3;++i){auto& buffer=buffers.values[i+1];host.allocator.invalidate(buffer,0,buffer.size);std::ofstream file(output/(std::to_string(i)+".bin"),std::ios::binary);file.write(static_cast<const char*>(buffer.mapped),static_cast<std::streamsize>(buffer.size));if(!file)throw std::runtime_error("Cannot save mirror output");}
        unsigned region=0,redHits=0,staticHits=0;
        const auto* colors=static_cast<const char*>(buffers.values[1].mapped);const auto* footprints=static_cast<const char*>(buffers.values[2].mapped);const auto* confidence=static_cast<const unsigned char*>(buffers.values[3].mapped);
        for(unsigned y=traceHeight*80/100;y<traceHeight*88/100;++y)for(unsigned x=traceWidth*30/100;x<traceWidth*70/100;++x){const auto index=y*traceWidth+x;++region;
            std::uint32_t packed=0;std::uint16_t attenuation=0;std::memcpy(&packed,colors+index*4,4);std::memcpy(&attenuation,footprints+index*4,2);
            const bool red=packed==0x380U&&confidence[index]>=128;redHits+=red;staticHits+=red&&attenuation==65535;}
        correct=redHits*4>=region*3&&staticHits*4>=region*3;
        report.update({{"regionPixels",region},{"redHits",redHits},{"staticHits",staticHits},{"minimumFraction",.75}});
    }
    report["validationErrors"]=host.errors.load();report["liveBuffers"]=host.allocator.statistics().liveBuffers;report["liveImages"]=host.allocator.statistics().liveImages;
    correct=correct&&host.errors==0&&host.allocator.statistics().liveBuffers==0&&host.allocator.statistics().liveImages==0;report["status"]=correct?"passed":"failed";
    std::ofstream file(output/"result.json");file<<report.dump(2)<<'\n';file.close();if(!file)throw std::runtime_error("Cannot save mirror report");
    std::cout<<report.dump()<<'\n';return correct?0:1;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
