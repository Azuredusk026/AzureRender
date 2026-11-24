#include "assets/AssetFormatRegistry.hpp"
#include "assets/AssetTransfer.hpp"
#include "assets/GltfLoader.hpp"
#include "runtime/AssetDatabase.hpp"
#include <assimp/Exporter.hpp>
#include <assimp/scene.h>
#include <chrono>
#include <fstream>
#include <iostream>
#include <cmath>

using namespace azurerender;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
void write(const std::filesystem::path& path,const std::string& bytes){std::filesystem::create_directories(path.parent_path());std::ofstream(path,std::ios::binary)<<bytes;}
template<class F> void rejects(F f,const char* message){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}require(rejected,message);}
std::unique_ptr<aiScene> animatedFixture() {
    auto scene=std::make_unique<aiScene>();scene->mRootNode=new aiNode("root");
    scene->mNumMaterials=2;scene->mMaterials=new aiMaterial*[2]{new aiMaterial(),new aiMaterial()};
    scene->mNumMeshes=2;scene->mMeshes=new aiMesh*[2];
    for(unsigned i=0;i<2;++i) {
        auto* mesh=scene->mMeshes[i]=new aiMesh();mesh->mPrimitiveTypes=aiPrimitiveType_TRIANGLE;mesh->mMaterialIndex=i;
        mesh->mNumVertices=3;mesh->mVertices=new aiVector3D[3]{{0,0,0},{2,0,0},{0,3,0}};
        mesh->mNormals=new aiVector3D[3]{{0,0,1},{0,0,1},{0,0,1}};
        mesh->mTextureCoords[0]=new aiVector3D[3]{{0,0,0},{1,0,0},{0,1,0}};mesh->mNumUVComponents[0]=2;
        mesh->mNumFaces=1;mesh->mFaces=new aiFace[1];mesh->mFaces[0].mNumIndices=3;mesh->mFaces[0].mIndices=new unsigned[3]{0,1,2};
        mesh->mNumBones=1;mesh->mBones=new aiBone*[1]{new aiBone()};auto* bone=mesh->mBones[0];bone->mName="joint";
        bone->mNumWeights=3;bone->mWeights=new aiVertexWeight[3]{{0,1},{1,1},{2,1}};
    }
    scene->mRootNode->mNumChildren=2;scene->mRootNode->mChildren=new aiNode*[2]{new aiNode("mesh"),new aiNode("joint")};
    for(unsigned i=0;i<2;++i)scene->mRootNode->mChildren[i]->mParent=scene->mRootNode;
    auto* node=scene->mRootNode->mChildren[0];node->mNumMeshes=2;node->mMeshes=new unsigned[2]{0,1};
    scene->mNumAnimations=1;scene->mAnimations=new aiAnimation*[1]{new aiAnimation()};auto* clip=scene->mAnimations[0];
    clip->mName="motion";clip->mDuration=1;clip->mTicksPerSecond=1;clip->mNumChannels=1;clip->mChannels=new aiNodeAnim*[1]{new aiNodeAnim()};
    auto* channel=clip->mChannels[0];channel->mNodeName="joint";channel->mNumPositionKeys=2;channel->mPositionKeys=new aiVectorKey[2]{{0,{0,0,0}},{1,{0,1,0}}};
    channel->mNumRotationKeys=2;channel->mRotationKeys=new aiQuatKey[2]{{0,aiQuaternion()},{1,aiQuaternion()}};
    channel->mNumScalingKeys=2;channel->mScalingKeys=new aiVectorKey[2]{{0,{1,1,1}},{1,{1,1,1}}};
    return scene;
}
}
int main(int argc,char** argv) {
    const auto root=std::filesystem::temp_directory_path()/std::filesystem::u8path(u8"azure 模型格式 "+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int status=0;
    try {
        const auto& registry=assetFormatRegistry();
        require(registry.require("obj").canRead,"OBJ must have a registered production reader");
        const auto source=root/std::filesystem::u8path(u8"三角形 模型.OBJ");
        write(source,"v 0 0 0\nv 2 0 0\nv 0 3 0\nf 1 2 3\n");
        AssetTransferRequest request;request.source=source;request.staging=root/"staging";request.destination=root/"published";request.formatId="obj";
        auto report=prepareAssetTransfer(request,registry,[] {return false;});
        require(report.summary.at("vertices").get<unsigned>()==3,"OBJ conversion must preserve its triangle vertices");
        const auto result=publishAssetTransfer(report);
        require(result.extension()==".glb","Model adapters must publish a canonical runtime container");
        const auto loaded=loadGltfAsset(result.u8string());
        require(loaded.indices.size()==3,"Canonical model must load through the production runtime loader");
        require(loaded.boundsMax[0]==2&&loaded.boundsMax[1]==3,"Conversion must preserve source coordinates");
        auto convert=[&](const std::string& id,const std::string& content){
            request.source=root/("fixture."+id);write(request.source,content);request.formatId=id;request.staging=root/(id+"-stage");request.destination=root/(id+"-target");
            auto converted=prepareAssetTransfer(request,registry,[] {return false;});auto path=publishAssetTransfer(converted);
            auto asset=loadGltfAsset(path.u8string());require(asset.indices.size()==3,"Interoperable format must preserve one triangle");
            require(asset.boundsMax[0]==2&&asset.boundsMax[1]==3,"Interoperable format must preserve coordinates");
        };
        convert("stl","solid triangle\nfacet normal 0 0 1\nouter loop\nvertex 0 0 0\nvertex 2 0 0\nvertex 0 3 0\nendloop\nendfacet\nendsolid triangle\n");
        convert("ply","ply\nformat ascii 1.0\nelement vertex 3\nproperty float x\nproperty float y\nproperty float z\nelement face 1\nproperty list uchar int vertex_indices\nend_header\n0 0 0\n2 0 0\n0 3 0\n3 0 1 2\n");
        for(const std::string id:{"dae","fbx"}) {
            auto scene=animatedFixture();Assimp::Exporter exporter;
            const auto* blob=exporter.ExportToBlob(scene.get(),id=="dae"?"collada":"fbxa");require(blob&&!blob->next,"Generated fixture must be a standalone redistributable container");
            request.source=root/("animated."+id);write(request.source,std::string(static_cast<const char*>(blob->data),blob->size));
            request.formatId=id;request.staging=root/(id+"-stage");request.destination=root/(id+"-target");
            request.maxSourceBytes=std::filesystem::file_size(request.source);
            auto prepared=prepareAssetTransfer(request,registry,[] {return false;});
            require(prepared.summary.at("materials").get<unsigned>()>=2,"Animated adapter must preserve independent material slots");
            require(prepared.summary.at("skinned").get<bool>()&&!prepared.summary.at("clips").empty(),"Animated adapter must preserve joints and clips");
            const auto asset=loadGltfAsset(publishAssetTransfer(prepared).u8string());
            require(asset.indices.size()==6&&asset.jointNodes.size()>0,"Runtime must consume both skinned primitives");
            require(asset.animations[0].endTime-asset.animations[0].startTime>0.9F,"Animation time must remain in seconds");
            const auto pose=sampleAnimationPose(asset,0,0.5F,false);
            require(!pose.jointMatrices.empty()&&std::abs(pose.jointMatrices[0][13]-0.5F)<0.001F,"Converted bind matrices and sampled channels must preserve half-second joint translation");
        }
        request.source=source;request.formatId="obj";request.staging=root/"cancel-stage";request.destination=root/"cancel-target";
        request.maxSourceBytes=512ULL*1024*1024;
        rejects([&]{prepareAssetTransfer(request,registry,[] {return true;});},"Cancelled conversion must reject without publication");
        require(!std::filesystem::exists(request.staging),"Cancellation must preserve temporary ownership");
        request.settings={{"unitScale",-1}};
        rejects([&]{prepareAssetTransfer(request,registry,[] {return false;});},"Invalid unit scales must fail");
        request.settings=nlohmann::json::object();
        {
            auto scene=animatedFixture();
            for(unsigned i=0;i<scene->mNumMeshes;++i){delete scene->mMeshes[i]->mBones[0];delete[] scene->mMeshes[i]->mBones;scene->mMeshes[i]->mBones=nullptr;scene->mMeshes[i]->mNumBones=0;}
            delete scene->mAnimations[0];delete[] scene->mAnimations;scene->mAnimations=nullptr;scene->mNumAnimations=0;
            aiMatrix4x4::Scaling(aiVector3D(-1,2,1),scene->mRootNode->mChildren[0]->mTransformation);
            Assimp::Exporter exporter;const auto* blob=exporter.ExportToBlob(scene.get(),"collada");require(blob,"Mirrored fixture must be generated");
            request.source=root/"mirrored.dae";write(request.source,std::string(static_cast<const char*>(blob->data),blob->size));request.formatId="dae";
            request.staging=root/"mirror-stage";request.destination=root/"mirror-target";
            auto mirrored=prepareAssetTransfer(request,registry,[] {return false;});const auto asset=loadGltfAsset(publishAssetTransfer(mirrored).u8string());
            require(asset.boundsMin[0]==-2&&asset.boundsMax[1]==6,"Hierarchy mirrors and nonuniform scaling must preserve world bounds");
            for(const auto& vertex:asset.vertices){const auto& n=vertex.normal;require(std::abs(n[0]*n[0]+n[1]*n[1]+n[2]*n[2]-1)<0.001F,"Transformed normals must remain unit length");}
        }
        request.source=source;request.formatId="obj";
        write(source,"mtllib absent.mtl\nv 0 0 0\nv 2 0 0\nv 0 3 0\nf 1 2 3\n");
        rejects([&]{prepareAssetTransfer(request,registry,[] {return false;});},"Missing material dependencies must fail visibly");
        write(source,"v 0 0 0\nv 2 0 0\nv 0 3 0\nf 1 2 3\n");
        Project::create(root/"project","Converted model");
        const auto project=Project::load(root/"project/project.azureproject");
        const auto target=root/"project/assets/converted.glb";
        std::filesystem::copy_file(result,target);
        auto generation=report.generation;
        const auto hash=generation.outputHashes.begin()->second;generation.outputHashes.clear();generation.outputHashes["converted.glb"]=hash;
        auto metadata=target;metadata+=".azmeta";
        write(metadata,nlohmann::json{{"schemaVersion",1},{"id","ffffffff-ffff-4fff-8fff-ffffffffffff"},{"settings",nlohmann::json::object()},{"dependencies",nlohmann::json::array()},{"importGeneration",generation.toJson()}}.dump());
        AssetDatabase database(project);database.refresh();
        require(database.idForPath("assets:/converted.glb")=="ffffffff-ffff-4fff-8fff-ffffffffffff","Converted provenance must support stable project asset identities");
        generation.generatorId="unknown.converter";
        write(metadata,nlohmann::json{{"schemaVersion",1},{"id","ffffffff-ffff-4fff-8fff-ffffffffffff"},{"settings",nlohmann::json::object()},{"dependencies",nlohmann::json::array()},{"importGeneration",generation.toJson()}}.dump());
        rejects([&]{database.refresh(true);},"Unregistered conversion provenance must be rejected");
        write(source,"v 0 0 0\nv 2 0 0\nv 0 3 0\nf 1 2 3\n");
        request.source=source;request.formatId="obj";request.staging=root/"scale-stage";request.destination=root/"scale-target";request.settings={{"unitScale",2}};
        auto scaled=prepareAssetTransfer(request,registry,[] {return false;});
        const auto scaledAsset=loadGltfAsset(publishAssetTransfer(scaled).u8string());
        require(scaledAsset.boundsMax[0]==4&&scaledAsset.boundsMax[1]==6,"Import unit scale must apply to runtime world geometry");
        request.settings=nlohmann::json::object();request.staging=root/"texture-stage";request.destination=root/"texture-target";
        std::string image(21,'\0');image[2]=2;image[12]=1;image[14]=1;image[16]=24;image[20]=static_cast<char>(255);write(root/"texture.tga",image);
        write(root/"material.mtl","newmtl red\nKd 1 0 0\nmap_Kd texture.tga\nnewmtl transparent\nKd 0 1 0\nd 0.5\n");
        write(source,"mtllib material.mtl\nv 0 0 0\nv 2 0 0\nv 0 3 0\nvt 0 0\nvt 1 0\nvt 0 1\nusemtl red\nf 1/1 2/2 3/3\nusemtl transparent\nf 1/1 3/3 2/2\n");
        auto textured=prepareAssetTransfer(request,registry,[] {return false;});
        require(textured.dependencies.size()>=2,"Model material and texture dependencies must be fingerprinted");
        const auto texturedPath=publishAssetTransfer(textured);std::filesystem::remove(root/"texture.tga");std::filesystem::remove(root/"material.mtl");
        const auto texturedAsset=loadGltfAsset(texturedPath.u8string());bool pixels=false,blend=false;
        for(const auto& material:texturedAsset.materials){pixels=pixels||!material.baseColorPixels.empty();blend=blend||material.alphaMode==AssetAlphaMode::Blend;}
        require(pixels&&blend&&texturedAsset.indices.size()==6,"Published GLB must own textures, transparency and independent slots");
        if(argc==2) {
            const auto output=std::filesystem::absolute(std::filesystem::u8path(argv[1]));std::filesystem::create_directories(output);
            for(const auto& item:std::filesystem::recursive_directory_iterator(root))if(item.is_regular_file()&&item.path().extension()==".glb") {
                const auto relative=item.path().lexically_relative(root);const auto target=output/relative;
                std::filesystem::create_directories(target.parent_path());std::filesystem::copy_file(item.path(),target,std::filesystem::copy_options::overwrite_existing);
            }
        }
        std::cout<<"Model format registration, UTF-8 OBJ conversion and canonical runtime geometry passed\n";
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';status=1;}
    std::filesystem::remove_all(root);return status;
}
