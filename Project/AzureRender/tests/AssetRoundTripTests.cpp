#include "assets/AssetTransfer.hpp"
#include "assets/image/ImageCodec.hpp"
#include "assets/audio/AudioCodec.hpp"
#include "assets/AssetExportJob.hpp"
#include "assets/model/ModelExportScene.hpp"
#include "assets/GltfLoader.hpp"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <thread>
using namespace azurerender;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class F>void rejects(F&& function,const char* message){bool rejected=false;try{function();}catch(const std::exception&){rejected=true;}require(rejected,message);}
void modelShape(const std::filesystem::path& path,unsigned& triangles,aiVector3D& minimum,aiVector3D& maximum){
    Assimp::Importer importer;const auto* scene=importer.ReadFile(path.u8string(),aiProcess_Triangulate|aiProcess_PreTransformVertices);
    if(!scene||!scene->mNumMeshes)throw std::runtime_error("Independent model reader: "+path.u8string()+": "+importer.GetErrorString());triangles=0;minimum={1e20F,1e20F,1e20F};maximum={-1e20F,-1e20F,-1e20F};
    for(unsigned m=0;m<scene->mNumMeshes;++m){const auto& mesh=*scene->mMeshes[m];triangles+=mesh.mNumFaces;
        for(unsigned i=0;i<mesh.mNumVertices;++i){const auto& v=mesh.mVertices[i];minimum.x=std::min(minimum.x,v.x);minimum.y=std::min(minimum.y,v.y);minimum.z=std::min(minimum.z,v.z);maximum.x=std::max(maximum.x,v.x);maximum.y=std::max(maximum.y,v.y);maximum.z=std::max(maximum.z,v.z);}}
}
void await(AssetExportJob& job){const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);while(!job.ready()&&std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(1));require(job.ready(),"Export worker completes within task deadline");}
}
int main(int argc,char** argv){if(argc!=5&&argc!=6)return 2;
    const auto root=argc==6?std::filesystem::u8path(argv[5]):std::filesystem::temp_directory_path()/("azure-roundtrip-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));int status=0;
    try{
        std::filesystem::create_directories(root);const auto& formats=assetFormatRegistry();
        for(const std::string id:{"gltf","glb","obj","stl","ply","png","jpeg","bmp","tga","hdr","exr","tiff","webp","wav","flac","vorbis"})
            require(formats.require(id).canWrite,"First-batch output formats require production writers");
        AssetTransferRequest request;request.operation=AssetTransferOperation::Export;
        request.source=std::filesystem::u8path(argv[2])/"alpha.png";request.formatId="jpeg";request.staging=root/"planned-stage";request.destination=root/"planned-output";request.operation=AssetTransferOperation::ExportPlan;
        const auto plan=prepareAssetTransfer(request,formats,{});require(!plan.losses.empty()&&!std::filesystem::exists(request.staging)&&!std::filesystem::exists(request.destination),"Export planning reports losses without creating output directories");request.operation=AssetTransferOperation::Export;
        const auto exportFile=[&](const std::filesystem::path& source,const std::string& format){request.source=source;request.formatId=format;request.staging=root/(format+"-stage");request.destination=root/(format+"-output");
            auto report=prepareAssetTransfer(request,formats,{});require(report.generation.generatorId=="azure."+format+"-export","Generation provenance identifies exports");const auto output=publishAssetTransfer(report);require(std::filesystem::is_regular_file(output),"Export publishes complete output");return std::make_pair(output,report);};
        unsigned originalTriangles;aiVector3D originalMinimum,originalMaximum;modelShape(std::filesystem::u8path(argv[1]),originalTriangles,originalMinimum,originalMaximum);
        for(const std::string id:{"gltf","glb","obj","stl","ply"}){
            const auto result=exportFile(std::filesystem::u8path(argv[1]),id);unsigned triangles;aiVector3D minimum,maximum;modelShape(result.first,triangles,minimum,maximum);
            require(triangles==originalTriangles&&(minimum-originalMinimum).Length()<1e-4F&&(maximum-originalMaximum).Length()<1e-4F,"Model export preserves triangles and transformed bounds");
            if(id=="obj"||id=="stl"||id=="ply")require(!result.second.losses.empty(),"Static exchange reports hierarchy, material and animation scope");
        }
        for(const std::string id:{"gltf","glb"}){
            request.source=std::filesystem::u8path(argv[4]);request.formatId=id;request.staging=root/("rig-"+id+"-stage");request.destination=root/("rig-"+id+"-output");auto report=prepareAssetTransfer(request,formats,{});const auto path=publishAssetTransfer(report);
            Assimp::Importer beforeReader,afterReader;const auto* before=beforeReader.ReadFile(request.source.u8string(),0);const auto* after=afterReader.ReadFile(path.u8string(),0);require(before&&after&&before->mNumAnimations>0&&after->mNumAnimations==before->mNumAnimations,"Container roundtrip preserves animation clip count");
            unsigned beforeBones=0,afterBones=0;for(unsigned m=0;m<before->mNumMeshes;++m)beforeBones+=before->mMeshes[m]->mNumBones;for(unsigned m=0;m<after->mNumMeshes;++m)afterBones+=after->mMeshes[m]->mNumBones;require(beforeBones>0&&afterBones==beforeBones,"Container roundtrip preserves skin joint bindings");
            for(unsigned a=0;a<before->mNumAnimations;++a){const auto& first=*before->mAnimations[a];const auto& second=*after->mAnimations[a];require(first.mName==second.mName&&first.mNumChannels==second.mNumChannels&&std::abs(first.mDuration/first.mTicksPerSecond-second.mDuration/second.mTicksPerSecond)<1e-5,"Container roundtrip preserves clip identity, channels and duration");}
            const auto sourceAsset=loadGltfAsset(request.source.u8string()),targetAsset=loadGltfAsset(path.u8string());
            require(sourceAsset.inverseBindMatrices.size()==targetAsset.inverseBindMatrices.size(),"Container retains inverse bind layout");
            for(std::size_t j=0;j<sourceAsset.inverseBindMatrices.size();++j)for(unsigned c=0;c<16;++c)require(std::abs(sourceAsset.inverseBindMatrices[j][c]-targetAsset.inverseBindMatrices[j][c])<1e-5F,"Container retains inverse bind transforms");
            for(std::size_t a=0;a<sourceAsset.animations.size();++a)for(float time:{0.F,.25F,.5F,.75F,1.F}){
                const auto sourcePose=sampleAnimationPose(sourceAsset,a,time,false),targetPose=sampleAnimationPose(targetAsset,a,time,false);require(sourcePose.jointMatrices.size()==targetPose.jointMatrices.size(),"Animation roundtrip retains pose layout");
                for(std::size_t j=0;j<sourcePose.jointMatrices.size();++j)for(unsigned c=0;c<16;++c)require(std::abs(sourcePose.jointMatrices[j][c]-targetPose.jointMatrices[j][c])<1e-5F,"Animation roundtrip preserves sampled joint motion");}

        }
        const auto textured=root/"texture-input";std::filesystem::create_directories(textured);std::filesystem::copy_file(std::filesystem::u8path(argv[2])/"alpha.png",textured/"pattern.png");
        std::ofstream(textured/"surface.mtl")<<"newmtl Surface\nKd 1 1 1\nmap_Kd pattern.png\n";std::ofstream(textured/"textured.obj")<<"mtllib surface.mtl\nv 0 0 0\nv 2 0 0\nv 0 3 0\nvt 0 0\nvt 1 0\nvt 0 1\nusemtl Surface\nf 1/1 2/2 3/3\n";
        std::vector<std::filesystem::path> texturedOutputs;
        for(const std::string id:{"glb","gltf","obj"}){request.source=textured/"textured.obj";request.formatId=id;request.staging=root/("textured-"+id+"-stage");request.destination=root/("textured-"+id+"-output");auto prepared=prepareAssetTransfer(request,formats,{});require(prepared.sourceFingerprints.size()>=3,"Texture provenance includes model, material and image dependencies");texturedOutputs.push_back(publishAssetTransfer(prepared));}
        std::filesystem::remove_all(textured);for(const auto& output:texturedOutputs){auto readable=output;if(output.extension()==".obj"){request.source=output;request.formatId="obj";request.operation=AssetTransferOperation::Import;request.staging=root/"obj-reimport-stage";request.destination=root/"obj-reimport-output";auto reimport=prepareAssetTransfer(request,formats,{});readable=publishAssetTransfer(reimport);request.operation=AssetTransferOperation::Export;}const auto loaded=loadGltfAsset(readable.u8string());bool pixels=false;for(const auto& material:loaded.materials)if(material.baseColorPixels==decodeImageFile(std::filesystem::u8path(argv[2])/"alpha.png").rgba8)pixels=true;require(pixels,"Exported model textures remain owned after all source dependencies are removed");}
        auto subtree=std::make_shared<ModelExportScene>();ModelExportNode group;group.id="assembly";group.transform[12]=10;subtree->nodes.push_back(group);
        ModelExportNode first;first.id="first";first.parent="assembly";first.source=std::filesystem::u8path(argv[1]);subtree->nodes.push_back(first);auto second=first;second.id="second";second.transform[12]=3;subtree->nodes.push_back(second);
        request.source=first.source;request.scene=subtree;request.formatId="glb";request.staging=root/"subtree-stage";request.destination=root/"subtree-output";auto subtreeReport=prepareAssetTransfer(request,formats,{});const auto subtreePath=publishAssetTransfer(subtreeReport);unsigned subtreeTriangles;aiVector3D subtreeMinimum,subtreeMaximum;modelShape(subtreePath,subtreeTriangles,subtreeMinimum,subtreeMaximum);
        require(subtreeTriangles==2*originalTriangles&&std::abs(subtreeMinimum.x-originalMinimum.x-10)<1e-4F&&std::abs(subtreeMaximum.x-originalMaximum.x-13)<1e-4F,"Scene subtree export combines repeated resources and parent transforms");
        Assimp::Importer subtreeReader;const auto* tree=subtreeReader.ReadFile(subtreePath.u8string(),0);require(tree&&tree->mRootNode->FindNode("assembly")&&tree->mRootNode->FindNode("first")&&tree->mRootNode->FindNode("second"),"Container subtree preserves authored node identities");request.scene.reset();
        request.scene=subtree;request.formatId="obj";request.staging=root/"static-subtree-stage";request.destination=root/"static-subtree-output";auto staticTree=prepareAssetTransfer(request,formats,{});modelShape(publishAssetTransfer(staticTree),subtreeTriangles,subtreeMinimum,subtreeMaximum);
        require(subtreeTriangles==2*originalTriangles&&std::abs(subtreeMinimum.x-originalMinimum.x-10)<1e-4F&&std::abs(subtreeMaximum.x-originalMaximum.x-13)<1e-4F,"Static subtree bakes repeated geometry and parent transforms");
        for(unsigned invalid=0;invalid<4;++invalid){auto broken=std::make_shared<ModelExportScene>(*subtree);
            if(invalid==0)broken->nodes[0].parent="first";if(invalid==1)broken->nodes[2].id="first";if(invalid==2)broken->nodes[1].parent="missing";if(invalid==3)broken->nodes[0].transform[0]=std::numeric_limits<float>::infinity();
            request.scene=broken;request.operation=AssetTransferOperation::ExportPlan;rejects([&]{prepareAssetTransfer(request,formats,{});},"Invalid subtree fails before output creation");}
        request.scene=subtree;request.maxSourceBytes=1;rejects([&]{prepareAssetTransfer(request,formats,{});},"Subtree honors bounded source ownership");request.maxSourceBytes=512ULL*1024*1024;request.scene.reset();request.operation=AssetTransferOperation::Export;
        const auto imageSource=std::filesystem::u8path(argv[2])/"alpha.png";const auto image=decodeImageFile(imageSource);
        for(const std::string id:{"png","jpeg","bmp","tga","hdr","exr","tiff","webp"}){
            const auto result=exportFile(imageSource,id);const auto decoded=decodeImageFile(result.first);require(decoded.width==image.width&&decoded.height==image.height,"Image exchange preserves dimensions");
            if(id=="png"||id=="tga"||id=="tiff"||id=="webp")require(decoded.rgba8==image.rgba8,"Lossless integer image exchange preserves all RGBA samples");
            if(id=="jpeg"||id=="hdr")require(!result.second.losses.empty(),"RGB output reports transparency loss");
            if(id=="jpeg"){double squared=0;for(std::size_t i=0;i<image.rgba8.size();++i)if(i%4!=3)squared+=std::pow(static_cast<double>(decoded.rgba8[i])-image.rgba8[i],2);require(std::sqrt(squared/(image.width*image.height*3))<8,"JPEG quality 100 preserves RGB within fixed eight-level RMS error");}
            if(id=="hdr"){const auto expected=imageLinearRgba(image);for(std::size_t i=0;i<expected.size();++i)if(i%4!=3)require(std::abs(decoded.rgba32[i]-expected[i])<.01F,"HDR preserves linear RGB within shared-exponent error");}
            if(id=="exr"){const auto expected=imageLinearRgba(image);require(decoded.rgba32.size()==expected.size(),"EXR preserves RGBA layout");for(std::size_t i=0;i<expected.size();++i)require(std::abs(decoded.rgba32[i]-expected[i])<1e-5F,"EXR preserves linear color and alpha");}
        }
        request.staging=root/"precision-stage";request.destination=root/"precision-output";request.source=std::filesystem::u8path(argv[2])/"gray16.png";request.formatId="png";
        auto precision=prepareAssetTransfer(request,formats,{});const auto high=decodeImageFile(publishAssetTransfer(precision));require(high.precision==ImagePrecision::UInt16&&high.rgba16==decodeImageFile(request.source).rgba16,"PNG exchange preserves all sixteen-bit samples");
        for(const std::string id:{"tiff","exr"}){request.source=std::filesystem::u8path(argv[2])/"linear.exr";request.formatId=id;request.staging=root/("float-"+id+"-stage");request.destination=root/("float-"+id+"-output");auto prepared=prepareAssetTransfer(request,formats,{});require(decodeImageFile(publishAssetTransfer(prepared)).rgba32==decodeImageFile(request.source).rgba32,"Floating image exchange preserves range, negatives and alpha exactly");}
        const auto audioSource=std::filesystem::u8path(argv[3])/"tone.wav";const auto audio=decodeAudioFile(audioSource);
        for(const std::string id:{"wav","flac","vorbis"}){
            const auto result=exportFile(audioSource,id);const auto decoded=decodeAudioFile(result.first);
            require(decoded.description.sampleRate==audio.description.sampleRate&&decoded.description.channels==audio.description.channels&&decoded.description.frames==audio.description.frames,"Audio exchange preserves native time and layout");
            if(id!="vorbis")require(decoded.samples==audio.samples,"Lossless fixture audio preserves independent PCM samples");
            else{double squared=0;for(std::size_t i=0;i<audio.samples.size();++i)squared+=std::pow(decoded.samples[i]-audio.samples[i],2);require(std::sqrt(squared/audio.samples.size())<.03,"Vorbis roundtrip stays within fixed RMS error");}
        }
        request.source=audioSource;request.formatId="wav";request.settings={{"sampleRate",24000},{"channels",2}};request.staging=root/"converted-audio-stage";request.destination=root/"converted-audio-output";
        auto convertedAudio=prepareAssetTransfer(request,formats,{});const auto stereo=decodeAudioFile(publishAssetTransfer(convertedAudio));require(stereo.description.sampleRate==24000&&stereo.description.channels==2&&std::abs(stereo.description.duration-audio.description.duration)<.001,"Export conversion preserves duration at the requested stereo rate");request.settings=nlohmann::json::object();
        request.source=imageSource;request.formatId="png";request.staging=root/"conflict-stage";request.destination=root/"png-output";
        auto report=prepareAssetTransfer(request,formats,{});const auto original=decodeImageFile(request.destination/report.entry).rgba8;
        rejects([&]{publishAssetTransfer(report);},"Occupied output requires explicit replacement");require(decodeImageFile(request.destination/"alpha.png").rgba8==original&&!std::filesystem::exists(request.staging),"Conflict preserves existing output and clears owned staging");
        request.destination=root/"cancel-output";rejects([&]{prepareAssetTransfer(request,formats,[]{return true;});},"Export respects cancellation");require(!std::filesystem::exists(request.staging)&&!std::filesystem::exists(request.destination),"Cancelled export leaves no output");
        request.settingsVersion=999;rejects([&]{prepareAssetTransfer(request,formats,{});},"Export rejects unknown settings version before writing");
        request.settingsVersion=1;request.settings={{"quality",0}};request.formatId="jpeg";request.operation=AssetTransferOperation::ExportPlan;rejects([&]{prepareAssetTransfer(request,formats,{});},"Invalid quality fails during planning");require(!std::filesystem::exists(request.staging),"Rejected plans never acquire staging");request.settings=nlohmann::json::object();request.operation=AssetTransferOperation::Export;
        const auto unicode=root/std::filesystem::u8path(u8"🎨 图像 来源.PNG");std::filesystem::copy_file(imageSource,unicode);request.source=unicode;request.formatId="png";request.staging=root/"job-stage";request.destination=root/"job-output";
        {AssetExportJob job(request);await(job);const auto output=job.finish();require(decodeImageFile(output).rgba8==image.rgba8&&job.progress()==1,"Background export publishes native Unicode output through the same codecs");}
        std::ofstream(request.destination/"keep.txt")<<"original destination";const auto originalFingerprint=assetTransferFingerprint(request.destination/"keep.txt");
        {AssetExportJob job(request);await(job);rejects([&]{job.finish();},"Background export preserves implicit overwrite protection");}require(assetTransferFingerprint(request.destination/"keep.txt")==originalFingerprint,"Rejected task preserves existing destination");
        {auto prepared=prepareAssetTransfer(request,formats,{});const auto backup=std::filesystem::path(request.staging.u8string()+".backup");rejects([&]{publishAssetTransfer(prepared,[&]{return std::filesystem::exists(backup);},true);},"Cancellation between backup and publication restores destination");require(assetTransferFingerprint(request.destination/"keep.txt")==originalFingerprint&&!std::filesystem::exists(backup)&&!std::filesystem::exists(request.staging),"Replacement cancellation restores the original directory and clears staging");}
        {AssetExportJob job(request);await(job);require(decodeImageFile(job.finish(true)).rgba8==image.rgba8&&!std::filesystem::exists(request.destination/"keep.txt"),"Explicit replacement publishes the new complete directory");}
        request.staging=root/"abandoned-stage";request.destination=root/"abandoned-output";{AssetExportJob job(request);await(job);require(std::filesystem::exists(request.staging),"Ready export owns its prepared output");}require(!std::filesystem::exists(request.staging)&&!std::filesystem::exists(request.destination),"Task destruction joins and discards unpublished output");
        std::ofstream(root/"blocked")<<"existing file";request.staging=root/"failure-stage";request.destination=root/"blocked/output";auto failed=prepareAssetTransfer(request,formats,{});rejects([&]{publishAssetTransfer(failed);},"Output I/O failure rejects publication");require(!std::filesystem::exists(request.staging)&&std::filesystem::is_regular_file(root/"blocked"),"I/O failure preserves existing file and cleans owned staging");
        auto customRegistry=formats;AssetFormatDescriptor custom;custom.id="portable-copy";custom.label="Portable copy";custom.domain=AssetDomain::Image;custom.extensions={".png"};custom.canWrite=true;
        custom.prepare=prepareAssetCopy;custom.plan=[](const AssetTransferRequest& input,const AssetCancellation&){AssetTransferReport result;result.entry=input.source.filename();result.summary={{"workflow","portable-copy"}};return result;};
        custom.normalizeExportSettings=[](const nlohmann::json& settings){if(!settings.is_object()||!settings.empty())throw std::invalid_argument("Portable copy requires empty settings");return settings;};
        custom.exportSettingsSchema={{"type","object"},{"additionalProperties",false}};customRegistry.add(std::move(custom));
        request.source=imageSource;request.formatId="portable-copy";request.staging=root/"custom-stage";request.destination=root/"custom-output";
        {AssetExportJob job(request,customRegistry);require(job.plan().summary.at("workflow")=="portable-copy","Registered planning belongs to the provider");await(job);require(decodeImageFile(job.finish()).rgba8==image.rgba8,"Registered writer runs without changing core dispatch");}
        std::cout<<"Sixteen writers, geometry, color, alpha, PCM, lossy budgets and recovery passed\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';status=1;}
    if(argc!=6)std::filesystem::remove_all(root);return status;
}
