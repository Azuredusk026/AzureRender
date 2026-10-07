#include "runtime/AnimationStateMachine.hpp"
#include "runtime/AudioRuntime.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
using namespace azurerender;
void check(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
int main(int argc,char** argv){
 if(argc!=2)return 2;
 try{
  auto machine=AnimationStateMachine::parse(nlohmann::json::parse(R"({"schemaVersion":1,"initial":"idle","states":[{"name":"idle","clip":0},{"name":"run","clip":1}],"transitions":[{"from":"idle","to":"run","parameter":"moving","value":true},{"from":"run","to":"idle","parameter":"moving","value":false}]})"));
  machine.advance(0.25);check(machine.state()=="idle" && machine.time()==0.25,"Idle time must advance");
  machine.set("moving",true);machine.advance(0.125);
  check(machine.state()=="run" && machine.clip()==1 && machine.time()==0.125,"Transition must switch clip and reset time");
  machine.set("moving",false);machine.advance(0.1);check(machine.state()=="idle","Reverse transition must apply");
  auto blended=AnimationStateMachine::parse(nlohmann::json::parse(R"({"schemaVersion":1,"initial":"idle","states":[{"name":"idle","clip":0},{"name":"walk","clip":1}],"transitions":[{"from":"idle","to":"walk","parameter":"moving","value":true,"blendSeconds":0.2}]})"));
  blended.advance(.4);blended.set("moving",true);blended.advance(.05);
  check(blended.previousClip()==0 && std::abs(blended.previousTime()-.45)<1e-9,"Crossfade must advance the outgoing clip");
  check(std::abs(blended.blend()-.25)<1e-6,"Crossfade must retain both poses during transition");
  blended.advance(.15);check(blended.blend()==1,"Crossfade must finish at configured duration");
  blended.select("idle",.2);blended.advance(.05,2);blended.select("walk",.2);
  check(blended.state()=="idle"&&std::abs(blended.blend()-.25)<1e-6,"Interrupted fades must preserve the current mixed pose");
  blended.advance(.15,2);
  check(blended.state()=="walk"&&blended.previousClip()==0&&blended.blend()==0,"Queued transition must begin from the completed outgoing pose");
  bool invalid=false;try{auto bad=AnimationStateMachine::parse(nlohmann::json::parse(R"({"schemaVersion":1,"initial":"missing","states":[],"transitions":[]})"));(void)bad;}catch(const std::exception&){invalid=true;}check(invalid,"Invalid state graph must be rejected");
  AudioRuntime audio(false);auto sound=audio.load(argv[1],false,0.5F);audio.play(sound);
  auto samples=audio.mix(480);double energy=0;for(float sample:samples)energy+=sample*sample;
  check(energy>0.01,"Real decoder and engine must produce audible samples");
  audio.pauseAll(true);samples=audio.mix(128);for(float sample:samples)check(std::abs(sample)<1e-6F,"Paused engine must produce silence");
  audio.pauseAll(false);audio.play(sound);check(audio.playing(sound),"Resume must retain usable sound");
  audio.release(sound);check(!audio.playing(sound),"Released sound must lose its handle");
  invalid=false;try{audio.load("missing.wav",false,1);}catch(const std::exception&){invalid=true;}check(invalid,"Missing audio must be rejected");
  std::cout<<"Animation transitions and real offline miniaudio decoding, playback, pause and release passed\n";
 }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
