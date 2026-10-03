#include "runtime/PresentationRuntime.hpp"
#include "runtime/LevelSession.hpp"
#include <iostream>
#include <stdexcept>
using namespace azurerender;
void check(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
int main(int argc,char** argv){
 if(argc!=2)return 2;
 try{
  RuntimeLifecycle runtime;LevelSession levels(Project::load(argv[1]),runtime);runtime.start();
  auto entity=runtime.entity("hero");
  runtime.world().addComponent(entity,game::Animator{"assets:/motion.json","idle",true});
  runtime.world().addComponent(entity,game::AudioSource{"assets:/portal.wav",false,false,0.5F,true});
  PresentationRuntime presentation(runtime,levels.assets(),false);
  presentation.update(0.1);check(presentation.animations().size()==1,"Animator must produce a renderer frame");
  runtime.world().tryGet<game::Animator>(entity)->state="run";presentation.play(entity);presentation.update(0.1);
  check(presentation.animations().at(0).clip==1,"Reflected state must drive renderer clip");
  check(presentation.audioStarts()==1 && presentation.mixedEnergy()>0.01,"Gameplay sound must reach real mixer");
  runtime.pause();auto time=presentation.animations().at(0).time;presentation.update(0);
  check(presentation.animations().at(0).time==time,"Paused animation must stay frozen");
  const auto sounds=presentation.soundCount();runtime.resume();runtime.world().destroyEntity(entity);presentation.update(0.1);
  check(presentation.animations().empty() && presentation.soundCount()+1==sounds,"Destroyed entities must release their presentation");
  levels.request("assets:/destination.azurelevel");check(levels.poll(),"Level must change");presentation.update(0.1);
  check(presentation.errors().empty(),"Public presentation assets must be valid");
  std::cout<<"World animation, real audio, pause, entity destruction and level replacement passed\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
