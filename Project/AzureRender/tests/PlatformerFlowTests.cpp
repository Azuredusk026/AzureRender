#include "app/ProjectRuntimeAssembly.hpp"
#include "runtime/ScriptRuntime.hpp"
#include "runtime/LevelSession.hpp"
#include "runtime/PresentationRuntime.hpp"
#include "assets/GltfLoader.hpp"
#include <fstream>
#include <iostream>
using namespace azurerender;
using Json=nlohmann::json;
void require(bool b,const std::string& message){if(!b)throw std::runtime_error(message);}
int main(int argc,char** argv){try{
    require(argc==3||argc==4,"Usage: PlatformerFlowTests project output [initial phase frames]");
    const auto project=Project::load(argv[1]);RuntimeLifecycle runtime;runtime.start();LevelSession levels(project,runtime);
    GameRuntime game(runtime,application::systems(),application::configuration(project));ScriptRuntime scripts(runtime,game,levels.assets());
    PresentationRuntime presentation(runtime,levels.assets(),false);std::map<std::string,std::string> ui;std::map<std::string,unsigned> audioPlays;std::string requested;
    scripts.setUiHandler([&](std::string id,std::string value){ui[id]=value;});
    scripts.setAudioHandler([&](auto entity){presentation.play(entity);++audioPlays[runtime.nodeId(entity)];});
    scripts.setLevelHandler([&](std::string value){requested=value;});
    game.setBeforeStep([&](double dt){scripts.update(dt);});game.setEventHandler([&](const auto& e){scripts.dispatch(e);});
    const auto designPath=std::filesystem::path(argv[1]).parent_path()/"PLATFORMER-DESIGN.json";std::ifstream designFile(designPath);Json design;designFile>>design;
    const int fps=15;unsigned frame=1;Json actions=Json::array(),landings=Json::array();std::map<int,bool> keys;
    const auto hero=[&]{return runtime.entity("hero");};
    const auto* settings=runtime.world().tryGet<game::Character>(hero());
    require(settings&&std::abs(settings->speed-4)<.001F&&std::abs(settings->sprintMultiplier-1.4F)<.001F,"Generated character must retain the measured movement parameters");
    require(runtime.world().tryGet<game::RigidBody>(runtime.entity("platform-3"))->kinematic&&runtime.world().tryGet<game::RigidBody>(runtime.entity("platform-5"))->kinematic,"Generated moving platforms must load as kinematic bodies");
    const auto rock=loadGltfAsset(levels.assets().resolveReference("assets:/environment/rock.glb").u8string());float rockTop=-1e9F;
    for(const auto& v:rock.vertices)rockTop=std::max(rockTop,v.position[1]);
    for(const auto& p:design["platforms"]){const auto cliff=runtime.entity("cliff-"+p["id"].get<std::string>().substr(9));
        if(cliff==ecs::kInvalidEntity)continue;const auto& transform=*runtime.world().tryGet<ecs::TransformComponent>(cliff);
        require(transform.translation[1]+rockTop*transform.scale[1]<=p["top"].get<float>()-.6F,"Visual island must stay below its walkable collision surface");}
    const auto position=[&]{return runtime.world().tryGet<ecs::TransformComponent>(hero())->translation;};
    const auto key=[&](int code,bool down){if(keys[code]==down)return;keys[code]=down;game.input().key(code,down);actions.push_back({{"frame",frame},{"action","key"},{"key",code},{"down",down}});};
    const auto tick=[&](int count){for(int i=0;i<count;++i){game.advance(1.0/fps);presentation.update(1.0/fps);++frame;require(scripts.errors().empty(),scripts.errors().empty()?"":scripts.errors().back());require(presentation.errors().empty(),"Presentation must load all assets");require(requested.empty(),"Completion replay retains independent level");}};
    tick(8+(argc==4?std::stoi(argv[3]):0));
    // A real walk off the side must recover at the start without reloading coins.
    key(68,true);tick(20);key(68,false);tick(40);
    require(std::abs(position()[0])<.4F&&std::abs(position()[2])<.4F,"Falling must restore the starting checkpoint");
    for(std::size_t index=0;index+1<design["platforms"].size();++index){
        const auto& source=design["platforms"][index];const auto& target=design["platforms"][index+1];
        const float edge=source["z"].get<float>()-source["halfLength"].get<float>();
        key(87,true);unsigned guard=0;
        while(position()[2]>edge+.43F&&guard++<180)tick(1);
        require(game.physics().grounded(hero()),"Jump starts on a supported platform");
        key(32,true);tick(1);key(32,false);
        const float targetZ=target["z"].get<float>();guard=0;
        while(position()[2]>targetZ+.35F&&guard++<180)tick(1);
        key(87,false);tick(16);
        auto p=position();require(std::abs(p[2]-targetZ)<.8F&&p[1]>target["top"].get<float>()-.1F&&game.physics().grounded(hero()),"Mandatory jump did not reach "+target["id"].get<std::string>()+" at "+Json(p).dump());
        landings.push_back({{"platform",target["id"]},{"position",p},{"frame",frame},{"grounded",true}});
        if(target.contains("motion")){
            const auto support=runtime.entity(target["id"].get<std::string>());const unsigned axis=index+1==3?0:1;
            const auto platformPosition=[&]{return runtime.world().tryGet<ecs::TransformComponent>(support)->translation;};
            const float relative=position()[axis]-platformPosition()[axis];float low=position()[axis],high=low;
            for(int i=0;i<30;++i){tick(1);low=std::min(low,position()[axis]);high=std::max(high,position()[axis]);
                require(game.physics().grounded(hero())&&std::abs(position()[axis]-platformPosition()[axis]-relative)<.06F,"Player must ride content-driven moving platforms while idle");}
            require(high-low>.05F,"Moving support and rider must actually translate during the observation");
        }
        if(index+1==4){
            const auto collected=runtime.world().tryGet<game::TaskState>(hero())->collected;
            key(68,true);tick(20);key(68,false);tick(40);
            require(std::abs(position()[2]-targetZ)<.4F&&std::abs(position()[0])<.4F,"Checkpoint fall must restore the saved platform");
            require(runtime.world().tryGet<game::TaskState>(hero())->collected==collected,"Checkpoint recovery retains coins");
        }
    }
    tick(15);auto* task=runtime.world().tryGet<game::TaskState>(hero());
    require(task&&task->completed,"Final gate must finish the level");require(task->collected>=6,"Main route must collect its guiding coins");
    require(presentation.audioStarts()>=15&&presentation.mixedEnergy()>0,"Jump, landing, coins, respawn and finish must mix audio");
    for(const auto* node:{"audio-jump","audio-land","audio-coin","audio-respawn","audio-finish"})require(audioPlays[node]>0,std::string("Missing sound: ")+node);
    require(audioPlays["audio-coin"]==8&&audioPlays["audio-finish"]==1&&audioPlays["audio-respawn"]==2,"One-shot feedback must fire once per event");
    const std::filesystem::path output(argv[2]);std::filesystem::create_directories(output);
    std::ofstream(output/"input.json")<<Json({{"schemaVersion",1},{"actions",actions}}).dump(2);
    std::ofstream(output/"summary.json")<<Json({{"passed",true},{"fps",fps},{"frames",frame+8},{"landings",landings},{"coins",task->collected},{"audioStarts",presentation.audioStarts()},{"audioEvents",audioPlays},{"mixedEnergy",presentation.mixedEnergy()},{"fallRecovery",true},{"ui",ui}}).dump(2);
    std::cout<<"Actual physics route completed with "<<task->collected<<" coins and "<<landings.size()<<" mandatory landings\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
