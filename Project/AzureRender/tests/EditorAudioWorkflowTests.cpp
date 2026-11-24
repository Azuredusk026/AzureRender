#include "editor/EditorSession.hpp"
#include "runtime/LevelSession.hpp"
#include "runtime/PresentationRuntime.hpp"
#include "runtime/AssetTypeRegistry.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>
using namespace azurerender;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
Json apply(EditorSession& session,const std::string& operation,Json parameters=Json::object()){
    const auto result=session.edit(operation,std::move(parameters));
    if(!result)throw std::runtime_error(operation+": "+result.diagnostics.dump());return result.value;
}
}
int main(int argc,char** argv){
    if(argc!=2&&argc!=3)return 2;
    const auto root=std::filesystem::temp_directory_path()/("azure-audio-workflow-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int status=0;
    try{
        Project::create(root,"Audio workflow");
        Json descriptor;{std::ifstream file(root/"project.azureproject");file>>descriptor;}
        descriptor["startupScene"]="assets:/audio.azurelevel";
        std::ofstream(root/"project.azureproject")<<descriptor.dump();
        std::ofstream(root/"assets/audio.azurelevel")<<Json{{"schemaVersion",1},{"id","audio"},{"sceneType","sample"},{"resources",Json::array()},{"nodes",Json::array()}}.dump();
        auto context=EditorContext::openProject(root/"project.azureproject");EditorSession session(context);
        std::vector<std::string> ids;
        for(const std::string extension:{"wav","flac","mp3","ogg"}){
            const auto source=root/("input."+extension);
            std::filesystem::copy_file(std::filesystem::u8path(argv[1])/("tone."+extension),source);
            const auto id=apply(session,"asset.import",{{"path",source.u8string()}}).get<std::string>();ids.push_back(id);
            require(assetTypeRegistry().classify(context->assets().resolve(id))->id=="audio","Public import classifies audio");
            require(context->scene().resources.empty(),"Audio import preserves model resource membership");
            apply(session,"node.create",{{"id","music-"+extension}});
            apply(session,"component.add",{{"type","azure.audio-source"}});
            apply(session,"component.fields",{{"type","azure.audio-source"},{"values",{{"asset",id},{"loop",true},{"autoplay",true},{"volume",.25},{"enabled",true}}}});
            apply(session,"document.save");
            const auto fingerprint=std::to_string(context->assets().records().at(id).fingerprint);
            apply(session,"asset.reimport",{{"asset",id},{"settings",Json::object()},{"baseFingerprint",fingerprint}});
            const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
            do{session.pollTasks();if(session.tasks().report().back().at("state")!="running")break;std::this_thread::sleep_for(std::chrono::milliseconds(1));}while(std::chrono::steady_clock::now()<deadline);
            require(session.tasks().report().back().at("state")=="completed","Public audio reimport completes");
            std::filesystem::remove(source);
        }
        apply(session,"document.save");
        auto reopened=EditorContext::openProject(root/"project.azureproject");
        for(const auto& id:ids)require(reopened->assets().resolve(id)==context->assets().resolve(id),"Audio identity survives save and reopen");
        const auto pack=root/std::filesystem::u8path(u8"移动 音频包");reopened->assets().writePack(pack);
        for(const auto& id:ids)require(decodeAudioFile(AssetDatabase::resolvePack(pack,id)).description.frames>0,"Packed audio decodes after source removal");
        RuntimeLifecycle runtime;LevelSession levels(Project::load(root/"project.azureproject"),runtime);runtime.start();
        PresentationRuntime presentation(runtime,levels.assets(),false);presentation.update(.1);
        require(presentation.errors().empty()&&presentation.soundCount()==4&&presentation.audioStarts()==4&&presentation.mixedEnergy()>1,"Reopened authored components reach actual offline mixer");
        runtime.pause();const auto energy=presentation.mixedEnergy();presentation.update(.1);
        require(presentation.mixedEnergy()==energy,"Paused authored audio produces silence");runtime.resume();
        runtime.world().destroyEntity(runtime.entity("music-wav"));presentation.update(.1);
        require(presentation.soundCount()==3,"Entity destruction releases audio ownership");
        runtime.replaceScene(SceneDocument{});presentation.update(.1);
        require(presentation.soundCount()==0,"Scene replacement releases all audio ownership");
        if(argc==3){const auto output=std::filesystem::u8path(argv[2]);require(!std::filesystem::exists(output),"Retained project destination must be new");std::filesystem::copy(root,output,std::filesystem::copy_options::recursive);}
        std::cout<<"Public four-format audio import, reimport, save, pack, playback and lifecycle passed\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';status=1;}
    std::filesystem::remove_all(root);return status;
}
