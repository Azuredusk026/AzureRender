#include "editor/EditorSession.hpp"
#include "runtime/PresentationRuntime.hpp"
#include "runtime/LevelSession.hpp"
#include "assets/image/ImageCodec.hpp"
#include "assets/image/ImageEncoder.hpp"
#include "assets/GltfLoader.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>
using namespace azurerender;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
Json apply(EditorSession& session,const std::string& id,Json args=Json::object()){
    auto result=session.edit(id,std::move(args));if(!result)throw std::runtime_error(id+": "+result.diagnostics.dump());return result.value;
}
Json wait(EditorSession& session,const std::string& id){
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(20);
    while(std::chrono::steady_clock::now()<deadline){session.pollTasks();for(const auto& row:session.tasks().report())if(row.at("id")==id&&row.at("state")!="running")return row;std::this_thread::sleep_for(std::chrono::milliseconds(1));}
    throw std::runtime_error("First batch task exceeded its deadline");
}
std::filesystem::path exportAsset(EditorSession& session,const std::string& id,const std::string& format,const std::filesystem::path& output){
    const auto plan=apply(session,"asset.export-plan",{{"asset",id},{"format",format},{"destination",output.u8string()}});
    const auto task=apply(session,"asset.export",{{"plan",plan.at("plan")}}).at("task").get<std::string>();const auto result=wait(session,task);
    require(result.at("state")=="completed","Reviewed asset export must complete");return std::filesystem::u8path(result.at("result").at("output").get<std::string>());
}
void reimport(EditorSession& session,const std::string& id,Json settings=Json::object()){
    const auto fingerprint=apply(session,"asset.inspect",{{"asset",id}}).at("fingerprint").get<std::string>();
    const auto task=apply(session,"asset.reimport",{{"asset",id},{"settings",std::move(settings)},{"baseFingerprint",fingerprint}}).at("task").get<std::string>();
    require(wait(session,task).at("state")=="completed","Authored references must survive public reimport");
}
std::shared_ptr<EditorContext> emptyProject(const std::filesystem::path& root,const std::string& name){
    Project::create(root,name);Json descriptor;{std::ifstream in(root/"project.azureproject");in>>descriptor;}
    descriptor["startupScene"]="assets:/creation.azurelevel";std::ofstream(root/"project.azureproject")<<descriptor.dump(2);
    std::ofstream(root/"assets/creation.azurelevel")<<Json{{"schemaVersion",1},{"id",name},{"sceneType","sample"},{"resources",Json::array()},{"nodes",Json::array()}}.dump(2);
    auto context=EditorContext::openProject(root/"project.azureproject");require(context->scene().nodes.empty(),"Authoring begins with an empty document");return context;
}
Json make(EditorSession& session,const std::filesystem::path& modelSource,const std::filesystem::path& imageSource,const std::filesystem::path& audioSource,float position){
    const auto model=apply(session,"asset.import",{{"path",modelSource.u8string()}}).get<std::string>();
    const auto image=apply(session,"asset.import",{{"path",imageSource.u8string()}}).get<std::string>();
    const auto audio=apply(session,"asset.import",{{"path",audioSource.u8string()}}).get<std::string>();
    apply(session,"node.place",{{"resource",model},{"id","authored-model"}});apply(session,"node.transform",{{"translation",{position,0,0}}});
    const auto material=apply(session,"material.create",{{"type","azure.pbr"},{"destination","assets:/authored.azurematerial"}}).at("asset").get<std::string>();
    apply(session,"material.field",{{"asset",material},{"field","textures.albedo"},{"value",{{"asset",image},{"usage","albedo"}}}});
    apply(session,"material.field",{{"asset",material},{"field","baseColor"},{"value",{1,1,1,1}}});
    apply(session,"material.field",{{"asset",material},{"field","roughness"},{"value",.35}});apply(session,"material.save",{{"asset",material}});
    const auto slot=apply(session,"material.slots",{{"node","authored-model"}}).at("slots").at(0).at("id").get<std::string>();
    apply(session,"material.assign",{{"nodes",{"authored-model"}},{"slotId",slot},{"material",material}});apply(session,"history.undo");apply(session,"history.redo");
    apply(session,"node.create",{{"id","authored-audio"}});apply(session,"component.add",{{"type","azure.audio-source"}});
    apply(session,"component.fields",{{"type","azure.audio-source"},{"values",{{"asset",audio},{"autoplay",true},{"loop",true},{"volume",.25},{"enabled",true}}}});
    apply(session,"document.save");
    const auto beforeImage=session.context().assets().records().at(image).fingerprint;
    const auto managedImage=session.context().assets().resolve(image);
    if(imageSource.extension()==".png"){
        std::ifstream bytes(managedImage,std::ios::binary);const std::string original((std::istreambuf_iterator<char>(bytes)),{});bytes.close();
        auto changed=decodeImageFile(managedImage);changed.rgba8.at(0)^=127;
        writeRgba8Png(managedImage,changed.width,changed.height,changed.rgba8.data());
        require(!session.edit("asset.reimport",{{"asset",image},{"settings",Json::object()},{"baseFingerprint",std::to_string(beforeImage)}}),"Tampered imported output is rejected by its generation integrity contract");
        std::ofstream restore(managedImage,std::ios::binary|std::ios::trunc);restore.write(original.data(),static_cast<std::streamsize>(original.size()));restore.close();
    }
    reimport(session,model);reimport(session,image,{{"texture",{{"filter","nearest"}}}});reimport(session,audio);apply(session,"document.save");
    require(session.context().assets().records().at(image).fingerprint!=beforeImage,"Changed import settings update the fingerprint while retaining identity");
    const auto slots=apply(session,"material.slots",{{"node","authored-model"}});require(slots.at("diagnostics").empty()&&slots.at("slots").at(0).at("override")==material,"Reimport retains stable authored material slots");
    const auto before=session.context().documentContent();require(!session.edit("material.assign",{{"nodes",{"authored-model","missing"}},{"slotId",slot},{"material",material}}),"Invalid batch assignment must fail");require(session.context().documentContent()==before,"Failed assignment preserves the authored document");
    const auto reopened=EditorContext::openProject(session.context().project().file);
    for(const auto& id:{model,image,audio,material})require(reopened->assets().resolve(id)==session.context().assets().resolve(id),"Stable resource identities survive reopen");
    auto loaded=loadGltfAsset(reopened->scene().resources.at(0).path.u8string());require(!loaded.vertices.empty(),"Authored model remains loadable");
    const auto node=std::find_if(reopened->scene().nodes.begin(),reopened->scene().nodes.end(),[](const auto& value){return value.id=="authored-model";});require(node!=reopened->scene().nodes.end(),"Authored node survives reopening");applyMaterialBindings(loaded,node->resolvedMaterials);
    require(std::any_of(loaded.materials.begin(),loaded.materials.end(),[&](const auto& value){return value.baseColorPixels==decodeImageFile(managedImage).rgba8;}),"Reopened authored material loads the current source texture samples");
    RuntimeLifecycle runtime;LevelSession levels(Project::load(reopened->project().file),runtime);runtime.start();PresentationRuntime presentation(runtime,levels.assets(),false);presentation.update(.1);
    require(presentation.audioStarts()==1&&presentation.mixedEnergy()>0&&presentation.errors().empty(),"Authored audio reaches actual offline playback");
    return {{"project",reopened->project().file.u8string()},{"model",model},{"image",image},{"audio",audio},{"material",material},{"slot",slot},{"mixedEnergy",presentation.mixedEnergy()}};
}
}
int main(int argc,char** argv){
    if(argc!=5&&argc!=6)return 2;
    const auto root=argc==6?std::filesystem::absolute(std::filesystem::u8path(argv[5])):std::filesystem::temp_directory_path()/("azure-first-batch-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));int status=0;
    try{
        require(!std::filesystem::exists(root),"Authoring evidence output must be new");std::filesystem::create_directories(root);
        std::filesystem::create_directories(root/"sources");const auto firstImage=root/"sources/alpha.png";std::filesystem::copy_file(std::filesystem::u8path(argv[2])/"alpha.png",firstImage);
        auto first=emptyProject(root/"workshop","workshop");EditorSession workshop(first);
        const auto firstIds=make(workshop,std::filesystem::u8path(argv[1]),firstImage,std::filesystem::u8path(argv[3])/"tone.wav",-1);
        const auto model=exportAsset(workshop,firstIds.at("model"),"glb",root/"exchange/model");
        const auto image=exportAsset(workshop,firstIds.at("image"),"webp",root/"exchange/image");
        const auto audio=exportAsset(workshop,firstIds.at("audio"),"flac",root/"exchange/audio");
        require(decodeImageFile(image).rgba8==decodeImageFile(first->assets().resolve(firstIds.at("image").get<std::string>())).rgba8,"Exchange preserves exact lossless image samples");
        auto second=emptyProject(root/"assembly","assembly");EditorSession assembly(second);const auto secondIds=make(assembly,model,image,audio,1);
        const auto rig=apply(assembly,"asset.import",{{"path",argv[4]}}).get<std::string>();apply(assembly,"node.place",{{"resource",rig},{"id","animated-rig"}});apply(assembly,"document.save");
        const auto graph=root/"assembly/assets/rig-animation.json";
        std::ofstream(graph)<<Json{{"schemaVersion",1},{"initial","idle"},{"states",Json::array({{{"name","idle"},{"clip",0},{"loop",true}}})},{"transitions",Json::array()}}.dump(2);
        apply(assembly,"assets.reload");apply(assembly,"node.select",{{"id","animated-rig"}});apply(assembly,"component.add",{{"type","azure.animator"}});apply(assembly,"component.fields",{{"type","azure.animator"},{"values",{{"asset","assets:/rig-animation.json"},{"state","idle"},{"startTime",.25}}}});apply(assembly,"document.save");
        apply(assembly,"animation.seek",{{"node","animated-rig"},{"state","idle"},{"time",.25}});apply(assembly,"animation.play",{{"node","animated-rig"}});assembly.advance(.1);require(apply(assembly,"animation.describe").at("animated-rig").at("time").get<double>()>.34&&!assembly.animationFrames().empty(),"Authored rig reaches actual production animation sampling");apply(assembly,"animation.pause",{{"node","animated-rig"}});
        const auto rigOutput=exportAsset(assembly,rig,"gltf",root/"exchange/rig");require(!loadGltfAsset(rigOutput.u8string()).animations.empty(),"Authored imported rig retains its animation clips through exchange");
        const auto secondModel=exportAsset(assembly,secondIds.at("model"),"obj",root/"exchange/static");const auto recovered=apply(workshop,"asset.import",{{"path",secondModel.u8string()}}).get<std::string>();require(!loadGltfAsset(first->assets().resolve(recovered).u8string()).vertices.empty(),"Second workflow exports a static model accepted by the first project");apply(workshop,"document.save");
        Json summary={{"passed",true},{"productionOperations",true},{"emptyProjects",2},{"workflow1",firstIds},{"workflow2",secondIds},{"stableReferences",true},{"materialSlots",true},{"losslessImageRoundTrip",true},{"actualOfflinePlayback",true},{"animationExchange",true},{"manualReview","P8"}};
        std::ofstream(root/"authoring-summary.json")<<summary.dump(2)<<'\n';std::cout<<summary.dump()<<'\n';
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';status=1;}
    if(argc!=6)std::filesystem::remove_all(root);return status;
}
