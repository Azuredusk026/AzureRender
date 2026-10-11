#include "assets/model/ModelFormatAdapter.hpp"
#include "assets/GltfLoader.hpp"
#include <assimp/Importer.hpp>
#include <assimp/Exporter.hpp>
#include <assimp/IOSystem.hpp>
#include <assimp/IOStream.hpp>
#include <assimp/ProgressHandler.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
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
    SourceIO(const AssetTransferRequest& r,AssetTransferReport& p,const AssetCancellation& c):request_(r),report_(p),cancelled_(c){}
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
        if(size>request_.maxSourceBytes||total_>request_.maxSourceBytes-added)throw std::runtime_error("Model dependencies exceed aggregate byte budget");
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
    // Embed external textures so the runtime container owns all image data.
    std::map<std::string,unsigned> embedded;
    for(unsigned m=0;m<scene->mNumMaterials;++m)for(unsigned t=1;t<=AI_TEXTURE_TYPE_MAX;++t) {
        auto type=static_cast<aiTextureType>(t);auto* material=scene->mMaterials[m];
        for(unsigned i=0;i<material->GetTextureCount(type);++i) {
            aiString path;material->GetTexture(type,i,&path);if(path.length&&path.C_Str()[0]=='*')continue;
            const std::string name=path.C_Str();unsigned index;
            if(embedded.count(name))index=embedded.at(name);else {
                auto bytes=io->snapshot(name.c_str());auto texture=std::make_unique<aiTexture>();
                texture->mWidth=static_cast<unsigned>(bytes.size());texture->mHeight=0;
                texture->pcData=new aiTexel[(bytes.size()+sizeof(aiTexel)-1)/sizeof(aiTexel)];std::memcpy(texture->pcData,bytes.data(),bytes.size());
                auto extension=assetExtension(std::filesystem::u8path(name));if(extension.size()>1)std::strncpy(texture->achFormatHint,extension.c_str()+1,8);
                auto** textures=new aiTexture*[scene->mNumTextures+1];for(unsigned j=0;j<scene->mNumTextures;++j)textures[j]=scene->mTextures[j];
                index=scene->mNumTextures++;textures[index]=texture.release();delete[] scene->mTextures;scene->mTextures=textures;embedded[name]=index;
            }
            aiString replacement("*"+std::to_string(index));material->AddProperty(&replacement,AI_MATKEY_TEXTURE(type,i));
        }
    }
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
        format.prepare=prepareModel;registry.add(std::move(format));
    }
}
}
