#include "runtime/PresentationRuntime.hpp"
#include "runtime/LevelSession.hpp"
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <thread>
using namespace azurerender;
void check(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
int main(int argc,char** argv){
 if(argc!=2)return 2;
 try{
  const auto graph=AnimationStateMachine::parse({{"schemaVersion",2},{"initial","idle"},
      {"states",{{{"name","idle"},{"clip",0}}}},{"transitions",nlohmann::json::array()},
      {"locomotion",{{"schemaVersion",1},{"samples",{
          {{"clip",0},{"speed",0},{"duration",2}},{{"clip",1},{"speed",1},{"duration",1}}}}}}});
  check(graph.locomotionProfile().has_value(),"Version two graph exposes its validated locomotion profile");
  for(int kind=0;kind<2;++kind){
      nlohmann::json invalid={{"schemaVersion",kind==0?1:3},{"initial","idle"},
          {"states",{{{"name","idle"},{"clip",0}}}},{"transitions",nlohmann::json::array()},
          {"locomotion",{{"schemaVersion",1},{"samples",{
              {{"clip",0},{"speed",0},{"duration",2}},{{"clip",1},{"speed",1},{"duration",1}}}}}}};
      bool rejected=false;try{(void)AnimationStateMachine::parse(invalid);}catch(const std::exception&){rejected=true;}
      check(rejected,"Version one locomotion extensions and unsupported graph versions must reject");
  }
  RuntimeLifecycle runtime;LevelSession levels(Project::load(argv[1]),runtime);runtime.start();
  AudioRuntime mixer(false);
  const auto handle=mixer.load(levels.assets().resolveReference("assets:/portal.wav"),true,0.5F);
  mixer.play(handle);
  mixer.mix(512);
  const auto cursor=mixer.position(handle);
  check(cursor>0,"The mixer must advance a playing source");
  mixer.setPaused(handle,true);
  mixer.pauseAll(true);mixer.pauseAll(false);
  mixer.setVolume(handle,0.25F);
  const auto silent=mixer.mix(512);
  check(mixer.position(handle)==cursor && std::all_of(silent.begin(),silent.end(),[](float v){return v==0;}),
        "A paused source must keep its position across global pause and gain changes");
  mixer.setPaused(handle,false);
  const auto resumed=mixer.mix(512);
  check(mixer.position(handle)>cursor && std::any_of(resumed.begin(),resumed.end(),[](float v){return v!=0;}),
        "Resuming a source must continue from its paused cursor");
  const auto source=mixer.description(handle);
  const auto finishFrames=static_cast<std::uint32_t>((source.duration+.1)*48000);
  mixer.mix(finishFrames);
  check(mixer.playing(handle) && mixer.position(handle)<source.frames,"A looping source must wrap its cursor");
  mixer.release(handle);
  const auto oneShot=mixer.load(levels.assets().resolveReference("assets:/portal.wav"),false,0.5F);
  mixer.play(oneShot);
  mixer.mix(finishFrames);
  check(!mixer.playing(oneShot) && mixer.position(oneShot)==source.frames,
        "A completed one-shot must stop at the last PCM frame");
  mixer.release(oneShot);
  auto entity=runtime.entity("hero");
  runtime.world().addComponent(entity,game::Animator{"assets:/motion.json","idle",true});
  runtime.world().addComponent(entity,game::AudioSource{"assets:/portal.wav",false,false,0.5F,true});
  PresentationRuntime presentation(runtime,levels.assets(),false);
  presentation.update(0.1);check(presentation.animations().size()==1,"Animator must produce a renderer frame");
  runtime.world().tryGet<game::Animator>(entity)->state="run";presentation.play(entity);presentation.update(0.1);
  check(presentation.animations().at(0).clip==1,"Reflected state must drive renderer clip");
  check(presentation.audioStarts()==1 && presentation.mixedEnergy()>0.01,"Gameplay sound must reach real mixer");
  const double energy=presentation.mixedEnergy();
  runtime.world().tryGet<game::AudioSource>(entity)->volume=0;
  presentation.update(.05);
  check(presentation.audioStarts()==1 && presentation.mixedEnergy()==energy,"A volume change must not replay the source");
  {
   PresentationRuntime recovery(runtime,levels.assets(),false);
   auto* audio=runtime.world().tryGet<game::AudioSource>(entity);
   audio->volume=.5F;
   const auto path=levels.assets().resolveReference("assets:/portal.wav");
   const auto unavailable=std::filesystem::path(path.string()+".unavailable");
   std::filesystem::rename(path,unavailable);
   try { recovery.update(0); }
   catch (...) { std::filesystem::rename(unavailable,path);throw; }
   std::filesystem::rename(unavailable,path);
   check(!recovery.errors().empty() && recovery.soundCount()==0,"Unavailable audio must retain a diagnostic");
   recovery.update(0);
   check(recovery.soundCount()==0,"Passive synchronization must keep failed sources bounded");
   recovery.play(entity);
   recovery.update(.1);
   check(recovery.audioStarts()==1 && recovery.mixedEnergy()>.01,
         "Explicit play must retry a source after its file becomes available");
  }
  runtime.pause();auto time=presentation.animations().at(0).time;presentation.update(0);
  check(presentation.animations().at(0).time==time,"Paused animation must stay frozen");
  const auto sounds=presentation.soundCount();runtime.resume();runtime.world().destroyEntity(entity);presentation.update(0.1);
  check(presentation.animations().empty() && presentation.soundCount()+1==sounds,"Destroyed entities must release their presentation");
  levels.preload("assets:/destination.azurelevel");
  const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
  do{levels.poll();std::this_thread::yield();}while(levels.loading() && std::chrono::steady_clock::now()<deadline);
  check(levels.preloaded("assets:/destination.azurelevel"),"Background candidate must become ready");
  levels.request("assets:/destination.azurelevel");check(levels.poll(),"Prepared level must change at the boundary");presentation.update(0.1);
  check(presentation.errors().empty(),"Public presentation assets must be valid");
  std::cout<<"World animation, real audio, pause, entity destruction and level replacement passed\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
