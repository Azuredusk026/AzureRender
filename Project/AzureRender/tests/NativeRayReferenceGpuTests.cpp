#include "support/VulkanComputeFixture.hpp"
#include "support/ComputeTexture.hpp"
#include "platform/NativeArguments.hpp"
#include "resources/BinaryFile.hpp"
#include "render/ProjectedReflectionRay.hpp"
#include <nlohmann/json.hpp>
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <set>

using namespace azurerender;
namespace {
using Json=nlohmann::json;
using Bytes=std::vector<char>;
constexpr std::uint64_t budget=128ULL*1024*1024;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
unsigned number(const Json& value,unsigned minimum,unsigned maximum){
    require(value.is_number_integer(),"Fixture requires integer fields");
    const auto result=value.get<std::int64_t>();
    require(result>=static_cast<std::int64_t>(minimum)&&result<=static_cast<std::int64_t>(maximum),"Fixture integer outside its range");return static_cast<unsigned>(result);
}
Bytes blob(const std::filesystem::path& root,const Json& row,std::uint64_t& total){
    const auto name=row.at("file").get<std::string>();
    require(!name.empty()&&name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.-")==std::string::npos,"Invalid fixture payload name");
    const auto path=std::filesystem::u8path(name);
    require(path==path.filename()&&path.extension()==".bin","Fixture payload must be a same-directory binary");
    const auto size=number(row.at("bytes"),1,static_cast<unsigned>(budget));
    require(total+size<=budget,"Fixture exceeds byte budget");
    require(!std::filesystem::is_symlink(root/path)&&std::filesystem::weakly_canonical(root/path).parent_path()==root,"Fixture payload escapes its directory");
    require(std::filesystem::file_size(root/path)==size,"Fixture payload length differs from its manifest");
    auto data=readBinaryFile((root/path).u8string());require(data.size()==size,"Incomplete fixture payload");total+=size;return data;
}
struct Format {const char* name;VkFormat value;unsigned bytes;};
constexpr std::array<Format,6> inputFormats{{
    {"R16G16B16A16_FLOAT",VK_FORMAT_R16G16B16A16_SFLOAT,8},{"R32_FLOAT",VK_FORMAT_R32_SFLOAT,4},
    {"R32_FLOAT",VK_FORMAT_R32_SFLOAT,4},{"R10G10B10A2_UNORM",VK_FORMAT_A2B10G10R10_UNORM_PACK32,4},
    {"R8_UNORM",VK_FORMAT_R8_UNORM,1},{"R10G10B10A2_UNORM",VK_FORMAT_A2B10G10R10_UNORM_PACK32,4}}};
constexpr std::array<Format,3> outputFormats{{
    {"R11G11B10_FLOAT",VK_FORMAT_B10G11R11_UFLOAT_PACK32,4},{"R16G16_UNORM",VK_FORMAT_R16G16_UNORM,4},{"R8_UNORM",VK_FORMAT_R8_UNORM,1}}};
constexpr std::array<const char*,3> outputNames{{"color","footprint","confidence"}};
struct TextureData {unsigned width=0,height=0,levels=0;Bytes data;};
struct Fixture {
    unsigned width=0,height=0,active=0;
    std::array<Bytes,4> buffers;
    std::array<TextureData,6> textures;
    std::array<Bytes,3> initial,reference;
};
Fixture loadFixture(const std::filesystem::path& file){
    require(std::filesystem::file_size(file)<=1024*1024,"Fixture manifest exceeds one MiB");
    std::ifstream stream(file);Json document;stream>>document;
    require(number(document.at("schemaVersion"),1,1)==1&&document.at("status")=="complete","Unsupported fixture schema or incomplete state");
    const auto root=std::filesystem::weakly_canonical(file.parent_path());Fixture result;std::uint64_t total=0;
    require(document.at("constants").size()==3&&document.at("textures").size()==6&&document.at("outputs").size()==3,"Fixture resource inventory is incomplete");
    constexpr std::array<unsigned,3> sizes{{1040,48,96}};
    for(unsigned i=0;i<3;++i){const auto& row=document.at("constants").at(i);require(number(row.at("index"),0,2)==i,"Wrong constant binding");
        result.buffers[i]=blob(root,row,total);require(result.buffers[i].size()==sizes[i],"Wrong constant block size");}
    const auto& tiles=document.at("tiles");require(number(tiles.at("stride"),4,4)==4,"Wrong tile stride");
    result.buffers[3]=blob(root,tiles,total);require(result.buffers[3].size()%4==0,"Misaligned tile data");
    result.active=number(tiles.at("activeCount"),0,65535);
    require(std::uint64_t(result.active)*4<=result.buffers[3].size(),"Tile count exceeds buffer capacity");
    require(document.at("dispatch")==Json::array({result.active,1,1})&&document.at("localSize")==Json::array({8,8,1}),"Wrong dispatch geometry");
    for(unsigned i=0;i<3;++i){const auto& row=document.at("outputs").at(i);
        require(number(row.at("binding"),0,2)==i&&row.at("format")==outputFormats[i].name&&row.at("role")==outputNames[i],"Wrong output binding or format");
        const auto width=number(row.at("width"),1,8192),height=number(row.at("height"),1,8192);
        require(number(row.at("texelBytes"),1,8)==outputFormats[i].bytes,"Wrong output texel size");
        if(i==0){result.width=width;result.height=height;}else require(width==result.width&&height==result.height,"Output extents differ");
        result.initial[i]=blob(root,row.at("initial"),total);result.reference[i]=blob(root,row.at("reference"),total);
        require(result.initial[i].size()==std::uint64_t(width)*height*outputFormats[i].bytes&&result.reference[i].size()==result.initial[i].size(),"Wrong output payload size");
    }
    std::array<float,2> dimensions{};std::memcpy(dimensions.data(),result.buffers[2].data(),sizeof(dimensions));
    require(dimensions[0]==float(result.width)&&dimensions[1]==float(result.height),"Constant extent differs from output extent");
    std::set<std::uint32_t> unique;
    for(unsigned i=0;i<result.active;++i){std::uint32_t tile=0;std::memcpy(&tile,result.buffers[3].data()+i*4,4);
        require((tile>>16)<(result.width+7)/8&&(tile&65535U)<(result.height+7)/8&&unique.insert(tile).second,"Duplicate or out-of-range tile");}
    for(unsigned i=0;i<6;++i){const auto& row=document.at("textures").at(i);auto& target=result.textures[i];
        require(number(row.at("binding"),1,6)==i+1&&row.at("format")==inputFormats[i].name,"Wrong sampled binding or format");
        target.width=number(row.at("width"),1,8192);target.height=number(row.at("height"),1,8192);
        const unsigned scale=(i==0||i==3)?2:1;
        require(target.width==result.width*scale&&target.height==result.height*scale,"Wrong sampled extent");
        require(number(row.at("texelBytes"),1,8)==inputFormats[i].bytes,"Wrong sampled texel size");
        unsigned maxLevels=1;for(auto size=std::max(target.width,target.height);size>1;size>>=1)++maxLevels;
        target.levels=static_cast<unsigned>(row.at("levels").size());require(target.levels>=1&&target.levels<=maxLevels,"Wrong mip count");
        std::uint64_t expected=0;
        for(unsigned mip=0;mip<target.levels;++mip){const auto& level=row.at("levels").at(mip);
            const auto width=std::max(1U,target.width>>mip),height=std::max(1U,target.height>>mip);
            require(number(level.at("mip"),0,13)==mip&&number(level.at("width"),1,8192)==width&&number(level.at("height"),1,8192)==height,"Wrong mip dimensions");
            require(level.at("offset")==expected&&level.at("bytes")==std::uint64_t(width)*height*inputFormats[i].bytes,"Wrong mip storage");
            expected+=std::uint64_t(width)*height*inputFormats[i].bytes;
        }
        target.data=blob(root,row,total);require(target.data.size()==expected,"Wrong sampled payload size");
    }
    return result;
}

class Storage {
public:
    rhi::IRhi& backend;rhi::GpuImage image{};VkImageView view{};
    Storage(rhi::IRhi& r,unsigned width,unsigned height,Format format,const Bytes& data):backend(r){
        rhi::GpuBuffer staging{};
        try{
            require(backend.supportsStorageImage(format.value),"Storage image format unavailable");
            staging=backend.allocator().createBuffer(data.size(),VK_BUFFER_USAGE_TRANSFER_SRC_BIT,true);
            std::memcpy(staging.mapped,data.data(),data.size());backend.allocator().flush(staging,0,staging.size);
            image=backend.allocator().createImage2D(width,height,format.value,VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_STORAGE_BIT);
            backend.transitionImageLayout(image,VK_IMAGE_LAYOUT_UNDEFINED,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,1);
            backend.copyBufferToImage(staging,image,width,height,1,format.bytes);
            backend.executeOneShot([&](rhi::ICommandRecorder& commands){commands.imageBarrier({image.image,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,VK_IMAGE_LAYOUT_GENERAL,1,
                VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT});});
            backend.allocator().destroyBuffer(staging);view=backend.createImageView(image.image,format.value,VK_IMAGE_ASPECT_COLOR_BIT,1);
        }catch(...){backend.allocator().destroyBuffer(staging);release();throw;}
    }
    ~Storage(){release();}
    Storage(const Storage&)=delete;Storage& operator=(const Storage&)=delete;
private:
    void release(){if(view)backend.destroyImageView(view);view={};backend.allocator().destroyImage(image);}
};
class Replay {
public:
    rhi::IRhi& backend;std::array<rhi::GpuBuffer,4> buffers{};std::array<rhi::GpuBuffer,3> readbacks{};
    std::array<std::unique_ptr<test::ComputeTexture>,6> sampled;
    std::array<std::unique_ptr<Storage>,3> outputs;
    VkSampler sampler{};VkDescriptorSetLayout descriptors{};VkDescriptorPool pool{};VkDescriptorSet set{};
    VkShaderModule shader{};VkPipelineLayout layout{};VkPipeline pipeline{};
    std::unique_ptr<ProjectedReflectionRayGpu> projected;
    Replay(rhi::IRhi& r,const Fixture& fixture,const Bytes& code,bool reverse,bool useProjected,bool ownProjected=true):backend(r){
        try{
            if(!useProjected){
            std::vector<rhi::DescriptorBindingDesc> bindings;
            for(unsigned i=0;i<3;++i)bindings.push_back({i,VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT});
            bindings.push_back({3,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT});
            for(unsigned i=4;i<10;++i)bindings.push_back({i,VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,1,VK_SHADER_STAGE_COMPUTE_BIT});
            bindings.push_back({10,VK_DESCRIPTOR_TYPE_SAMPLER,1,VK_SHADER_STAGE_COMPUTE_BIT});
            for(unsigned i=11;i<14;++i)bindings.push_back({i,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,1,VK_SHADER_STAGE_COMPUTE_BIT});
            descriptors=backend.createDescriptorSetLayout(bindings);rhi::DescriptorPoolDesc desc;
            desc.sizes={{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,3},{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1},{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,6},{VK_DESCRIPTOR_TYPE_SAMPLER,1},{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,3}};
            pool=backend.createDescriptorPool(desc);set=backend.allocateDescriptorSets(pool,descriptors,1).at(0);
            layout=backend.createPipelineLayout(descriptors,nullptr);shader=backend.createShaderModule(code);pipeline=backend.createComputePipeline({shader,layout});
            rhi::SamplerDesc settings;settings.filter=VK_FILTER_NEAREST;settings.mipmapLinear=false;settings.maxLod=1000;
            settings.addressU=settings.addressV=settings.addressW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;sampler=backend.createSampler(settings);
            backend.writeDescriptorImage({set,10,VK_DESCRIPTOR_TYPE_SAMPLER,VK_NULL_HANDLE,sampler,VK_IMAGE_LAYOUT_UNDEFINED});
            }
            for(unsigned i=0;i<4;++i){auto data=fixture.buffers[i];
                if(useProjected&&i<3)continue;
                if(reverse&&i==3&&fixture.active){std::vector<std::uint32_t> words(fixture.active);std::memcpy(words.data(),data.data(),words.size()*4);
                    std::reverse(words.begin(),words.end());std::memcpy(data.data(),words.data(),words.size()*4);}
                const auto type=i==3?VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                buffers[i]=backend.allocator().createBuffer(data.size(),i==3?VK_BUFFER_USAGE_STORAGE_BUFFER_BIT:VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,true);
                std::memcpy(buffers[i].mapped,data.data(),data.size());backend.allocator().flush(buffers[i],0,buffers[i].size);
                if(!useProjected)backend.writeDescriptorBuffer({set,i,type,buffers[i].buffer,buffers[i].size});
            }
            for(unsigned i=0;i<6;++i){const auto& data=fixture.textures[i];sampled[i]=std::make_unique<test::ComputeTexture>(backend,data.width,data.height,data.levels,inputFormats[i].value,data.data,inputFormats[i].bytes);
                if(!useProjected)backend.writeDescriptorImage({set,4+i,VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,sampled[i]->view,VK_NULL_HANDLE,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});}
            for(unsigned i=0;i<3;++i){outputs[i]=std::make_unique<Storage>(backend,fixture.width,fixture.height,outputFormats[i],fixture.initial[i]);
                readbacks[i]=backend.allocator().createBuffer(fixture.reference[i].size(),VK_BUFFER_USAGE_TRANSFER_DST_BIT,true);
                if(!useProjected)backend.writeDescriptorImage({set,11+i,VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,outputs[i]->view,VK_NULL_HANDLE,VK_IMAGE_LAYOUT_GENERAL});}
            if(useProjected&&ownProjected)projected=std::make_unique<ProjectedReflectionRayGpu>(backend,2,code);
        }catch(...){release();throw;}
    }
    ~Replay(){release();}
    Replay(const Replay&)=delete;Replay& operator=(const Replay&)=delete;
private:
    void release(){
        projected.reset();
        if(pool)backend.destroyDescriptorPool(pool);pool={};if(pipeline)backend.destroyPipeline(pipeline);pipeline={};
        if(shader)backend.destroyShaderModule(shader);shader={};if(layout)backend.destroyPipelineLayout(layout);layout={};
        if(descriptors)backend.destroyDescriptorSetLayout(descriptors);descriptors={};if(sampler)backend.destroySampler(sampler);sampler={};
        for(auto& item:sampled)item.reset();for(auto& item:outputs)item.reset();
        for(auto& item:buffers)backend.allocator().destroyBuffer(item);for(auto& item:readbacks)backend.allocator().destroyBuffer(item);
    }
};
void recordProjected(Replay& replay,rhi::ICommandRecorder& commands,const Fixture& fixture,ProjectedReflectionRayGpu& kernel,unsigned slot){
    // Source-only mapping stays in the test adapter. Core consumes semantic view fields.
    ProjectedReflectionView view;
    const auto columns=[&](auto& target,unsigned start){std::memcpy(target.data(),fixture.buffers[0].data()+start*16,sizeof(target));};
    columns(view.worldNormalToView,0);columns(view.viewDirectionToClip,8);columns(view.clipToView,12);
    columns(view.relativeWorldToClip,20);columns(view.clipToWorld,24);columns(view.previousClipToWorld,61);
    std::memcpy(view.position.data(),fixture.buffers[0].data()+44*16,12);
    std::memcpy(view.depthReciprocal.data(),fixture.buffers[1].data()+40,8);
    const auto constant=[&](unsigned row,unsigned component){float value=0;std::memcpy(&value,fixture.buffers[2].data()+row*16+component*4,4);return value;};
    const auto seed=constant(1,0),steps=constant(3,0);
    require(std::isfinite(seed)&&seed>=0&&seed<=16777215&&std::trunc(seed)==seed&&std::isfinite(steps)&&steps>=1&&steps<=4096&&std::trunc(steps)==steps,"Invalid source sample settings");
    ProjectedReflectionSettings settings;settings.jitterSeed=static_cast<unsigned>(seed);settings.maxSteps=static_cast<unsigned>(steps);
    settings.thickness=constant(1,1);settings.motionBlend=constant(2,2);settings.maximumFootprintLevel=constant(5,1);
    ProjectedReflectionInputs inputs;
    inputs.previousColor=replay.sampled[0]->view;inputs.previousDepth=replay.sampled[1]->view;inputs.currentDepth=replay.sampled[2]->view;
    inputs.motion=replay.sampled[3]->view;inputs.surfaceMask=replay.sampled[4]->view;inputs.normalRoughness=replay.sampled[5]->view;
    inputs.sourceExtent={fixture.textures[0].width,fixture.textures[0].height};inputs.traceExtent={fixture.width,fixture.height};
    inputs.tiles=replay.buffers[3].buffer;inputs.tileBytes=replay.buffers[3].size;inputs.tileCount=fixture.active;
    const ProjectedReflectionOutputs outputs{replay.outputs[0]->view,replay.outputs[1]->view,replay.outputs[2]->view,{fixture.width,fixture.height}};
    kernel.record(commands,slot,inputs,outputs,view,settings);
}
Fixture maskedFixture(const Fixture& source){
    Fixture result;result.width=17;result.height=9;result.active=6;
    for(unsigned i=0;i<3;++i)result.buffers[i]=source.buffers[i];result.buffers[3]=Bytes(24,0);
    for(unsigned y=0;y<2;++y)for(unsigned x=0;x<3;++x){const auto tile=(x<<16)|y;std::memcpy(result.buffers[3].data()+(y*3+x)*4,&tile,4);}
    const std::array<float,4> dimensions{{17,9,1.F/17,1.F/9}};std::memcpy(result.buffers[2].data(),dimensions.data(),16);
    for(unsigned i=0;i<6;++i){auto& texture=result.textures[i];texture.width=(i==0||i==3)?35:17;texture.height=(i==0||i==3)?19:9;
        texture.levels=1;texture.data=Bytes(texture.width*texture.height*inputFormats[i].bytes,0);}
    const std::array<std::uint32_t,2> initial{{0x781e03c0U,0x8000ffffU}};
    for(unsigned i=0;i<3;++i){result.initial[i]=Bytes(153*outputFormats[i].bytes,0);result.reference[i]=result.initial[i];
        if(i<2)for(unsigned p=0;p<153;++p)std::memcpy(result.initial[i].data()+p*4,&initial[i],4);}
    return result;
}
void readback(Replay& replay,const Fixture& fixture,rhi::ICommandRecorder& commands){
    for(unsigned i=0;i<3;++i){const auto image=replay.outputs[i]->image.image;const auto& buffer=replay.readbacks[i];
        commands.imageBarrier({image,VK_IMAGE_LAYOUT_GENERAL,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,1,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_ACCESS_TRANSFER_WRITE_BIT|VK_ACCESS_SHADER_WRITE_BIT|VK_ACCESS_SHADER_READ_BIT,VK_ACCESS_TRANSFER_READ_BIT});
        commands.copyImageToBuffer(image,buffer.buffer,{fixture.width,fixture.height});
        commands.bufferBarrier({buffer.buffer,0,buffer.size,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_HOST_BIT,VK_ACCESS_TRANSFER_WRITE_BIT,VK_ACCESS_HOST_READ_BIT});}
}
bool compare(Replay& replay,const Fixture& fixture,test::VulkanComputeFixture& host,const std::filesystem::path& output,const std::string& prefix,Json& report){
    bool exact=true;report=Json::array();
    for(unsigned i=0;i<3;++i){const auto& buffer=replay.readbacks[i];host.allocator.invalidate(buffer,0,buffer.size);const auto* bytes=static_cast<const char*>(buffer.mapped);
        std::uint64_t differences=0;for(std::size_t index=0;index<fixture.reference[i].size();++index)differences+=bytes[index]!=fixture.reference[i][index];exact=exact&&differences==0;
        report.push_back({{"role",outputNames[i]},{"bytes",buffer.size},{"differentBytes",differences}});
        std::ofstream file(output/(prefix+outputNames[i]+".bin"),std::ios::binary);file.write(bytes,static_cast<std::streamsize>(buffer.size));require(bool(file),"Cannot write replay output");}
    return exact;
}
}
int main(int argc,char** argv)try{
    const auto arguments=nativeArguments(argc,argv);require(arguments.size()>=4&&arguments.size()<=6,"Expected shader, fixture, new output and optional --reverse/--projected");
    bool reverse=false,useProjected=false;
    for(std::size_t i=4;i<arguments.size();++i){if(arguments[i]=="--reverse"&&!reverse)reverse=true;else if(arguments[i]=="--projected"&&!useProjected)useProjected=true;else require(false,"Unknown or duplicate replay option");}
    const auto fixture=loadFixture(std::filesystem::u8path(arguments[2]));
    const auto code=readBinaryFile(arguments[1]);std::uint32_t magic=0;if(code.size()>=4)std::memcpy(&magic,code.data(),4);
    require(code.size()>=20&&code.size()<=1024*1024&&code.size()%4==0&&magic==0x07230203U,"Invalid reference SPIR-V");
    const auto output=std::filesystem::absolute(std::filesystem::u8path(arguments[3]));require(std::filesystem::create_directory(output),"Replay output must be a new directory");
    test::VulkanComputeFixture host;require(fixture.active<=host.properties.limits.maxComputeWorkGroupCount[0],"Dispatch exceeds device limit");
    Json report={{"status","failed"},{"device",host.properties.deviceName},{"reverse",reverse},{"projected",useProjected},{"outputs",Json::array()}};bool exact=true;
    {
        Replay replay(*host.backend,fixture,code,reverse,useProjected);
        Fixture secondaryFixture;std::unique_ptr<Replay> secondary;
        if(useProjected){secondaryFixture=maskedFixture(fixture);secondary=std::make_unique<Replay>(*host.backend,secondaryFixture,code,false,true,false);}
        host.submit([&](rhi::ICommandRecorder& commands,VkCommandBuffer){
            for(unsigned i=0;i<4;++i)if(replay.buffers[i].buffer)commands.bufferBarrier({replay.buffers[i].buffer,0,replay.buffers[i].size,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_ACCESS_HOST_WRITE_BIT,static_cast<VkAccessFlags>(i==3?VK_ACCESS_SHADER_READ_BIT:VK_ACCESS_UNIFORM_READ_BIT)});
            if(useProjected){
                recordProjected(replay,commands,fixture,*replay.projected,0);
                const auto& buffer=secondary->buffers[3];commands.bufferBarrier({buffer.buffer,0,buffer.size,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_ACCESS_HOST_WRITE_BIT,VK_ACCESS_SHADER_READ_BIT});
                recordProjected(*secondary,commands,secondaryFixture,*replay.projected,1);
            }
            else if(fixture.active){commands.bindComputePipeline(replay.pipeline);commands.bindComputeDescriptorSet(replay.layout,replay.set);commands.dispatch(fixture.active,1,1);}
            readback(replay,fixture,commands);if(secondary)readback(*secondary,secondaryFixture,commands);
        });
        exact=compare(replay,fixture,host,output,"",report["outputs"]);
        if(secondary){const bool otherExact=compare(*secondary,secondaryFixture,host,output,"secondary-",report["secondaryOutputs"]);exact=exact&&otherExact;report["independentFrameSlots"]=otherExact;}
    }
    report["validationErrors"]=host.errors.load();report["liveBuffers"]=host.allocator.statistics().liveBuffers;report["liveImages"]=host.allocator.statistics().liveImages;
    exact=exact&&host.errors==0&&host.allocator.statistics().liveBuffers==0&&host.allocator.statistics().liveImages==0;report["status"]=exact?"passed":"failed";
    std::ofstream stream(output/"result.json");stream<<report.dump(2)<<'\n';stream.close();require(bool(stream),"Cannot write replay report");
    std::cout<<report.dump()<<'\n';return exact?0:1;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
