#include "assets/model/ModelFormatAdapter.hpp"
#include "assets/GltfLoader.hpp"
#include "assets/model/ModelExportScene.hpp"
#include "assets/image/ImageCodec.hpp"
#include <stb_image_write.h>
#include <assimp/Importer.hpp>
#include <assimp/Exporter.hpp>
#include <assimp/IOSystem.hpp>
#include <assimp/IOStream.hpp>
#include <assimp/ProgressHandler.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/config.h>
#include <assimp/SceneCombiner.h>
#include <fstream>
#include <cstring>
#include <cmath>

namespace azurerender { namespace {
void check(const AssetCancellation& c){if(c&&c())throw std::runtime_error("Model conversion cancelled");}
class MemoryStream final:public Assimp::IOStream {
    std::vector<char> bytes_;std::size_t cursor_=0;
public:
    explicit MemoryStream(std::vector<char> bytes):bytes_(std::move(bytes)){}
    std::size_t Read(void* output,std::size_t size,std::size_t count) override {
        if(!size)return 0;count=std::min(count,(bytes_.size()-cursor_)/size);
        std::memcpy(output,bytes_.data()+cursor_,count*size);cursor_+=count*size;return count;
    }
    std::size_t Write(const void*,std::size_t,std::size_t) override{return 0;}
    aiReturn Seek(std::size_t offset,aiOrigin origin) override {
        std::size_t position=offset;
        if(origin==aiOrigin_CUR){if(offset>bytes_.size()-cursor_)return aiReturn_FAILURE;position=cursor_+offset;}
        if(origin==aiOrigin_END){if(offset>bytes_.size())return aiReturn_FAILURE;position=bytes_.size()-offset;}
        if(position>bytes_.size())return aiReturn_FAILURE;cursor_=position;return aiReturn_SUCCESS;
    }
    std::size_t Tell() const override{return cursor_;}
    std::size_t FileSize() const override{return bytes_.size();}
    void Flush() override{}
};
class SourceIO final:public Assimp::IOSystem {
    const AssetTransferRequest& request_;AssetTransferReport& report_;const AssetCancellation& cancelled_;
    std::uint64_t total_=0;
public:
    SourceIO(const AssetTransferRequest& r,AssetTransferReport& p,const AssetCancellation& c):request_(r),report_(p),cancelled_(c){
        for(const auto& input:report_.sourceFingerprints){const auto size=std::filesystem::file_size(input.first);if(size>request_.maxSourceBytes||total_>request_.maxSourceBytes-size)throw std::runtime_error("Model dependencies exceed aggregate byte budget");total_+=size;}}
    std::filesystem::path resolve(const char* name) const {
        auto path=std::filesystem::u8path(name);if(!path.is_absolute())path=request_.source.parent_path()/path;
        path=std::filesystem::weakly_canonical(path);
        auto relative=path.lexically_relative(std::filesystem::weakly_canonical(request_.source.parent_path()));
        if(relative.empty()||relative.is_absolute()||*relative.begin()=="..")throw std::runtime_error("Model dependency escapes source directory");
        return path;
    }
    bool Exists(const char* name)const override{return std::filesystem::is_regular_file(resolve(name));}
    char getOsSeparator()const override{return '/';}
    std::vector<char> snapshot(const char* name) {
        check(cancelled_);const auto path=resolve(name);
        if(!std::filesystem::is_regular_file(path))throw std::runtime_error("Missing model dependency: "+path.u8string());
        auto size=std::filesystem::file_size(path);
        const auto previous=report_.sourceFingerprints.find(path);
        const auto added=previous==report_.sourceFingerprints.end()?size:0;
        if(size>request_.maxSourceBytes||added>request_.maxSourceBytes||total_>request_.maxSourceBytes-added)throw std::runtime_error("Model dependencies exceed aggregate byte budget");
        std::ifstream input(path,std::ios::binary);std::vector<char> bytes(static_cast<std::size_t>(size));
        for(std::size_t offset=0;offset<bytes.size();) {
            check(cancelled_);auto count=std::min<std::size_t>(65536,bytes.size()-offset);
            input.read(bytes.data()+offset,static_cast<std::streamsize>(count));if(!input)throw std::runtime_error("Model dependency read failed");offset+=count;
        }
        const auto hash=generationHash(std::string(bytes.begin(),bytes.end()));
        if(previous!=report_.sourceFingerprints.end()&&previous->second!=hash)throw std::runtime_error("Model dependency changed during conversion");
        if(previous==report_.sourceFingerprints.end())total_+=size;
        report_.sourceFingerprints[path]=hash;
        const auto relative=path.lexically_relative(std::filesystem::weakly_canonical(request_.source.parent_path())).generic_u8string();
        report_.generation.inputHashes[relative]=hash;
        if(path!=std::filesystem::weakly_canonical(request_.source)&&previous==report_.sourceFingerprints.end()) {
            report_.dependencies.push_back(path);report_.generation.dependencyHashes[relative]=hash;
        }
        return bytes;
    }
    Assimp::IOStream* Open(const char* name,const char* mode="rb")override {
        if(std::strchr(mode,'w')||std::strchr(mode,'+'))return nullptr;
        return new MemoryStream(snapshot(name));
    }
    void Close(Assimp::IOStream* stream)override{delete stream;}
};
class Progress final:public Assimp::ProgressHandler {
    AssetCancellation cancelled_;
public:explicit Progress(AssetCancellation c):cancelled_(std::move(c)){}
    bool Update(float)override{return !cancelled_||!cancelled_();}
};
void embedTextures(aiScene& scene,SourceIO& io) {
    // Embed external textures so the runtime container owns all image data.
    std::map<std::string,unsigned> embedded;
    for(unsigned m=0;m<scene.mNumMaterials;++m)for(unsigned t=1;t<=AI_TEXTURE_TYPE_MAX;++t) {
        auto type=static_cast<aiTextureType>(t);auto* material=scene.mMaterials[m];
        for(unsigned i=0;i<material->GetTextureCount(type);++i) {
            aiString path;material->GetTexture(type,i,&path);if(!path.length||path.C_Str()[0]=='*')continue;
            const std::string name=path.C_Str();unsigned index;
            if(embedded.count(name))index=embedded.at(name);else {
                auto bytes=io.snapshot(name.c_str());auto texture=std::make_unique<aiTexture>();
                texture->mWidth=static_cast<unsigned>(bytes.size());texture->mHeight=0;
                texture->pcData=new aiTexel[(bytes.size()+sizeof(aiTexel)-1)/sizeof(aiTexel)];std::memcpy(texture->pcData,bytes.data(),bytes.size());
                auto extension=assetExtension(std::filesystem::u8path(name));if(extension.size()>1)std::strncpy(texture->achFormatHint,extension.c_str()+1,8);
                auto** textures=new aiTexture*[scene.mNumTextures+1];for(unsigned j=0;j<scene.mNumTextures;++j)textures[j]=scene.mTextures[j];
                index=scene.mNumTextures++;textures[index]=texture.release();delete[] scene.mTextures;scene.mTextures=textures;embedded[name]=index;
            }
            aiString replacement("*"+std::to_string(index));material->AddProperty(&replacement,AI_MATKEY_TEXTURE(type,i));
        }
    }
}
AssetTransferReport prepareModel(const AssetTransferRequest& request,const AssetCancellation& cancelled) {
    AssetTransferReport report;Assimp::Importer importer;
    auto* io=new SourceIO(request,report,cancelled);importer.SetIOHandler(io);importer.SetProgressHandler(new Progress(cancelled));
    const auto* imported=importer.ReadFile(std::filesystem::absolute(request.source).u8string(),aiProcess_Triangulate|aiProcess_GenSmoothNormals|aiProcess_CalcTangentSpace|aiProcess_ValidateDataStructure);
    if(!imported)throw std::runtime_error(std::string("Model import failed: ")+importer.GetErrorString());
    check(cancelled);std::unique_ptr<aiScene> scene(importer.GetOrphanedScene());
    if(!scene->mRootNode||!scene->mNumMeshes)throw std::runtime_error("Model contains no renderable geometry");
    const auto scale=request.settings.value("unitScale",1.0);const auto axis=request.settings.value("upAxis",std::string("y"));
    for(auto it=request.settings.begin();it!=request.settings.end();++it)if(it.key()!="unitScale"&&it.key()!="upAxis")throw std::invalid_argument("Unknown model import setting");
    if(!std::isfinite(scale)||scale<=0||scale>1000000||(axis!="y"&&axis!="z"))throw std::invalid_argument("Invalid model axis or unit scale");
    aiMatrix4x4 transform;aiMatrix4x4::Scaling(aiVector3D(static_cast<float>(scale)),transform);
    if(axis=="z"){aiMatrix4x4 rotation;aiMatrix4x4::RotationX(-1.57079632679F,rotation);transform=transform*rotation;}
    scene->mRootNode->mTransformation=transform*scene->mRootNode->mTransformation;
    embedTextures(*scene,*io);
    report.losses={"Source constraints, procedural shading nodes and animation curve interpolation are represented by the imported sampled scene"};
    if(scene->mNumCameras||scene->mNumLights)report.losses.push_back("Source cameras and lights require separate scene authoring");
    Assimp::Exporter exporter;const auto* blob=exporter.ExportToBlob(scene.get(),"glb2");
    if(!blob||blob->next)throw std::runtime_error(std::string("Canonical GLB export failed: ")+exporter.GetErrorString());
    check(cancelled);report.entry=request.source.stem();report.entry+=".glb";
    std::ofstream output(request.staging/report.entry,std::ios::binary);output.write(static_cast<const char*>(blob->data),static_cast<std::streamsize>(blob->size));output.close();
    if(!output)throw std::runtime_error("Canonical GLB write failed");
    report.outputs.push_back(report.entry);report.generation.outputHashes[report.entry.generic_u8string()]=generationHash(std::string(static_cast<const char*>(blob->data),blob->size));
    const auto loaded=loadGltfAsset((request.staging/report.entry).u8string());
    report.summary={{"vertices",loaded.vertices.size()},{"indices",loaded.indices.size()},{"materials",loaded.materials.size()},{"joints",loaded.jointNodes.size()},{"skinned",loaded.hasSkin},{"clips",nlohmann::json::array()},{"losses",report.losses},{"adapter","assimp-6.0.4"}};
    for(const auto& clip:loaded.animations)report.summary["clips"].push_back({{"name",clip.name},{"duration",clip.endTime-clip.startTime},{"channels",clip.channels.size()}});
    return report;
}
std::unique_ptr<aiScene> loadExportScene(const AssetTransferRequest& request,AssetTransferReport& report,const AssetCancellation& cancel){
    if(!request.scene){
        Assimp::Importer importer;auto* io=new SourceIO(request,report,cancel);importer.SetIOHandler(io);importer.SetProgressHandler(new Progress(cancel));
        if(!importer.ReadFile(std::filesystem::absolute(request.source).u8string(),aiProcess_Triangulate|aiProcess_ValidateDataStructure))throw std::runtime_error(std::string("Model export source failed: ")+importer.GetErrorString());
        std::unique_ptr<aiScene> scene(importer.GetOrphanedScene());if(!scene->mRootNode||!scene->mNumMeshes)throw std::runtime_error("Model contains no exportable geometry");embedTextures(*scene,*io);check(cancel);return scene;
    }
    const auto& nodes=request.scene->nodes;if(nodes.empty()||nodes.size()>4096)throw std::invalid_argument("Export subtree requires one to 4096 nodes");
    std::map<std::string,const ModelExportNode*> definitions;std::map<std::string,std::unique_ptr<aiNode>> owned;std::map<std::string,std::vector<aiNode*>> children;bool primary=false;
    for(const auto& node:nodes){check(cancel);if(node.id.empty()||node.id.size()>128||!definitions.emplace(node.id,&node).second)throw std::invalid_argument("Export subtree node identity is invalid or duplicated");
        for(float value:node.transform)if(!std::isfinite(value))throw std::invalid_argument("Export subtree transform must be finite");
        if(node.transform[3]!=0||node.transform[7]!=0||node.transform[11]!=0||node.transform[15]!=1)throw std::invalid_argument("Export subtree requires affine transforms");
        owned.emplace(node.id,std::make_unique<aiNode>(node.id));const auto& m=node.transform;owned.at(node.id)->mTransformation=aiMatrix4x4(m[0],m[4],m[8],m[12],m[1],m[5],m[9],m[13],m[2],m[6],m[10],m[14],m[3],m[7],m[11],m[15]);
        if(!node.source.empty()&&std::filesystem::weakly_canonical(node.source)==std::filesystem::weakly_canonical(request.source))primary=true;
    }
    if(!primary)throw std::invalid_argument("Export subtree must include its primary source");
    for(const auto& node:nodes){auto parent=node.parent;std::size_t depth=0;while(!parent.empty()){if(!definitions.count(parent)||++depth>nodes.size())throw std::invalid_argument("Export subtree parent is missing or cyclic");parent=definitions.at(parent)->parent;}children[node.parent].push_back(owned.at(node.id).get());}
    std::map<std::string,std::unique_ptr<aiNode*[]>> arrays;
    for(const auto& group:children){auto array=std::make_unique<aiNode*[]>(group.second.size());std::copy(group.second.begin(),group.second.end(),array.get());arrays.emplace(group.first,std::move(array));}
    auto master=std::make_unique<aiScene>();std::string rootName="AzureExport";while(definitions.count(rootName))rootName+="_";master->mRootNode=new aiNode(rootName);
    for(const auto& group:children){auto* parent=group.first.empty()?master->mRootNode:owned.at(group.first).get();parent->mNumChildren=static_cast<unsigned>(group.second.size());parent->mChildren=arrays.at(group.first).release();for(auto* child:group.second)child->mParent=parent;}
    for(auto& node:owned)node.second.release();
    std::vector<std::unique_ptr<aiScene>> sources;std::vector<Assimp::AttachmentInfo> attachments;sources.reserve(nodes.size());attachments.reserve(nodes.size());
    report.summary["scene"]=nlohmann::json::array();
    for(const auto& node:nodes){report.summary["scene"].push_back({{"id",node.id},{"parent",node.parent},{"source",node.source.generic_u8string()},{"transform",node.transform}});if(node.source.empty())continue;
        auto source=request;source.scene.reset();source.source=node.source;auto imported=loadExportScene(source,report,cancel);auto* parent=master->mRootNode->FindNode(node.id.c_str());attachments.emplace_back(imported.get(),parent);sources.push_back(std::move(imported));}
    if(attachments.empty())throw std::invalid_argument("Export subtree has no geometry resources");
    check(cancel);aiScene* combined=nullptr;auto* rawMaster=master.release();for(auto& source:sources)source.release();
    Assimp::SceneCombiner::MergeScenes(&combined,rawMaster,attachments,AI_INT_MERGE_SCENE_GEN_UNIQUE_NAMES|AI_INT_MERGE_SCENE_GEN_UNIQUE_NAMES_IF_NECESSARY|AI_INT_MERGE_SCENE_GEN_UNIQUE_MATNAMES);
    std::unique_ptr<aiScene> result(combined);if(!result||!result->mNumMeshes)throw std::runtime_error("Export subtree combination failed");check(cancel);return result;
}
std::vector<std::string> modelExportLosses(const std::string& format){
    if(format=="glb"||format=="gltf")return {"Custom source extensions and engine project properties require project-format storage"};
    return {"Static exchange bakes node transforms and exports bind-pose geometry; hierarchy, skin and animation are represented as static geometry",
        format=="obj"?"OBJ stores basic MTL materials and converts textures to 8-bit PNG; PBR extensions are omitted":"STL and PLY omit material textures and PBR shading"};
}
struct TextureOutput {std::vector<unsigned char> bytes;AssetCancellation cancel;};
void textureWrite(void* context,void* data,int count){auto& output=*static_cast<TextureOutput*>(context);check(output.cancel);if(count<0||static_cast<std::size_t>(count)>64ULL*1024*1024-output.bytes.size())throw std::runtime_error("Exported texture exceeds 64 MiB");const auto* bytes=static_cast<unsigned char*>(data);output.bytes.insert(output.bytes.end(),bytes,bytes+count);}
void exportObjTextures(aiScene& scene,const AssetTransferRequest& request,AssetTransferReport& report,const AssetCancellation& cancel){
    std::map<std::pair<const aiTexture*,bool>,std::filesystem::path> outputs;
    for(unsigned m=0;m<scene.mNumMaterials;++m)for(unsigned t=1;t<=AI_TEXTURE_TYPE_MAX;++t){auto* material=scene.mMaterials[m];const auto type=static_cast<aiTextureType>(t);
        for(unsigned slot=0;slot<material->GetTextureCount(type);++slot){check(cancel);aiString reference;material->GetTexture(type,slot,&reference);if(!reference.length)continue;
            const auto* texture=scene.GetEmbeddedTexture(reference.C_Str());if(!texture)throw std::runtime_error("OBJ export texture is not owned by its source scene");
            const bool color=type==aiTextureType_DIFFUSE||type==aiTextureType_BASE_COLOR||type==aiTextureType_EMISSIVE||type==aiTextureType_EMISSION_COLOR;
            const auto key=std::make_pair(texture,color);auto found=outputs.find(key);
            if(found==outputs.end()){
                DecodedImage image;
                if(texture->mHeight==0){const auto* bytes=reinterpret_cast<const unsigned char*>(texture->pcData);image=decodeImageBytes(std::vector<unsigned char>(bytes,bytes+texture->mWidth),cancel);}
                else{if(texture->mWidth==0||texture->mHeight==0||texture->mWidth>8192||texture->mHeight>8192)throw std::runtime_error("Raw embedded texture exceeds dimensions");image.width=texture->mWidth;image.height=texture->mHeight;image.sourceChannels=4;image.rgba8.resize(static_cast<std::size_t>(image.width)*image.height*4);
                    for(std::size_t i=0;i<static_cast<std::size_t>(image.width)*image.height;++i){image.rgba8[i*4]=texture->pcData[i].r;image.rgba8[i*4+1]=texture->pcData[i].g;image.rgba8[i*4+2]=texture->pcData[i].b;image.rgba8[i*4+3]=texture->pcData[i].a;}}
                const auto converted=convertImageToRgba8(image,color);report.losses.insert(report.losses.end(),converted.losses.begin(),converted.losses.end());TextureOutput output{{},cancel};
                if(!stbi_write_png_to_func(textureWrite,&output,image.width,image.height,4,converted.pixels.data(),image.width*4))throw std::runtime_error("OBJ texture PNG encoding failed");
                const auto relative=std::filesystem::path("texture-"+std::to_string(outputs.size())+".png");std::ofstream file(request.staging/relative,std::ios::binary);file.write(reinterpret_cast<const char*>(output.bytes.data()),output.bytes.size());file.close();if(!file)throw std::runtime_error("OBJ texture write failed");
                report.outputs.push_back(relative);report.generation.outputHashes[relative.generic_u8string()]=assetTransferFingerprint(request.staging/relative,cancel);found=outputs.emplace(key,relative).first;
            }
            aiString replacement(found->second.generic_u8string());material->AddProperty(&replacement,AI_MATKEY_TEXTURE(type,slot));
        }
    }
}
}
void registerModelFormats(AssetFormatRegistry& registry) {
    for(const std::string id:{"obj","stl","ply","dae","fbx"}) {
        AssetFormatDescriptor format;format.id=id;format.label=id;format.extensions={"."+id};format.canRead=true;format.transformsSource=true;
        format.features={"geometry","materials","canonical-glb","axis-unit-settings","loss-report"};
        format.probe=[id](const std::vector<unsigned char>& bytes){
            const std::string text(bytes.begin(),bytes.end());
            if(id=="obj")return text.find("v ")!=std::string::npos&&text.find("f ")!=std::string::npos;
            if(id=="ply")return text.rfind("ply",0)==0;
            if(id=="dae")return text.find("<COLLADA")!=std::string::npos;
            if(id=="fbx")return text.rfind("Kaydara FBX Binary",0)==0||text.find("FBXHeaderExtension")!=std::string::npos;
            return text.rfind("solid",0)==0||bytes.size()>=84;
        };
        format.canWrite=id=="obj"||id=="stl"||id=="ply";
        if(format.canWrite){format.plan=inspectModelExport;format.normalizeExportSettings=normalizeModelExportSettings;format.exportSettingsSchema={{"type","object"},{"properties",nlohmann::json::object()},{"additionalProperties",false}};}
        format.prepare=[](const AssetTransferRequest& request,const AssetCancellation& cancel){return request.operation==AssetTransferOperation::Export?prepareModelExport(request,cancel):prepareModel(request,cancel);};registry.add(std::move(format));
    }
}
AssetTransferReport prepareModelExport(const AssetTransferRequest& request,const AssetCancellation& cancelled){
    if(!request.settings.empty())throw std::invalid_argument("Model export settings must be empty");
    AssetTransferReport report;auto scene=loadExportScene(request,report,cancelled);
    const bool container=request.formatId=="glb"||request.formatId=="gltf";
    report.losses=modelExportLosses(request.formatId);if(request.formatId=="obj")exportObjTextures(*scene,request,report,cancelled);
    check(cancelled);Assimp::Exporter exporter;
    report.entry=request.source.stem();report.entry+="."+request.formatId;
    Assimp::ExportProperties properties;properties.SetPropertyString(AI_CONFIG_EXPORT_BLOB_NAME,report.entry.generic_u8string());
    const auto format=request.formatId=="glb"?"glb2":request.formatId=="gltf"?"gltf2":request.formatId.c_str();
    const auto* blob=exporter.ExportToBlob(scene.get(),format,container?0U:aiProcess_PreTransformVertices,&properties);if(!blob)throw std::runtime_error(std::string("Model export failed: ")+exporter.GetErrorString());
    std::uint64_t total=0;for(const auto& output:report.outputs){const auto size=std::filesystem::file_size(request.staging/output);if(size>512ULL*1024*1024-total)throw std::runtime_error("Model textures exceed output budget");total+=size;}
    for(auto* part=blob;part;part=part->next){check(cancelled);const auto relative=part==blob?report.entry:std::filesystem::u8path(part->name.C_Str());
        if(relative.empty()||relative.is_absolute()||relative.has_root_name()||relative.lexically_normal()!=relative||*relative.begin()=="..")throw std::runtime_error("Model exporter produced an invalid dependency path");
        if(part->size>512ULL*1024*1024-total)throw std::runtime_error("Model output exceeds 512 MiB");total+=part->size;
        std::filesystem::create_directories((request.staging/relative).parent_path());std::ofstream output(request.staging/relative,std::ios::binary);
        const auto* data=static_cast<const char*>(part->data);for(std::size_t offset=0;offset<part->size;){check(cancelled);const auto count=std::min<std::size_t>(65536,part->size-offset);output.write(data+offset,count);offset+=count;}
        output.close();if(!output)throw std::runtime_error("Model output write failed");report.outputs.push_back(relative);report.generation.outputHashes[relative.generic_u8string()]=assetTransferFingerprint(request.staging/relative,cancelled);
    }
    report.summary.update({{"domain","model"},{"meshes",scene->mNumMeshes},{"materials",scene->mNumMaterials},{"animations",scene->mNumAnimations},{"losses",report.losses},{"adapter","assimp-6.0.4"}});return report;
}
AssetTransferReport inspectModelExport(const AssetTransferRequest& request,const AssetCancellation& cancelled){
    AssetTransferReport report;auto scene=loadExportScene(request,report,cancelled);
    report.losses=modelExportLosses(request.formatId);
    report.summary.update({{"domain","model"},{"meshes",scene->mNumMeshes},{"materials",scene->mNumMaterials},{"animations",scene->mNumAnimations},{"losses",report.losses}});check(cancelled);return report;
}
nlohmann::json normalizeModelExportSettings(const nlohmann::json& settings){if(!settings.is_object()||!settings.empty())throw std::invalid_argument("Model export settings must be empty");return settings;}
}
