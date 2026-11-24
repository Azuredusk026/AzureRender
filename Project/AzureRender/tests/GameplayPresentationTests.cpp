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
  const auto timeline=nlohmann::json::parse(R"({
    "schemaVersion":3,"initial":"enter","states":[{"name":"enter","clip":0},{"name":"turn","clip":1},{"name":"settled","clip":2}],
    "transitions":[],"timeline":[{"from":"enter","to":"turn","afterSeconds":0.8,"blendSeconds":0.25,"startSeconds":0.2},
                                {"from":"turn","to":"settled","afterSeconds":0.5,"blendSeconds":0.1}]})");
  auto authored=AnimationStateMachine::parse(timeline);
  authored.advance(.39,2);
  check(authored.state()=="enter"&&std::abs(authored.time()-.78)<1e-9,"Timed transition started before the source clip boundary");
  authored.advance(.06,2);
  check(authored.state()=="turn"&&std::abs(authored.time()-.3)<1e-9&&std::abs(authored.previousTime()-.9)<1e-9,
        "Crossing the clip boundary must preserve both sample times and the authored target offset");
  check(std::abs(authored.blend()-.2)<1e-6,"Timed fade must use only the frame portion after its source boundary");
  authored.advance(.23,2);
  check(authored.state()=="settled"&&std::abs(authored.time()-.06)<1e-9&&std::abs(authored.blend()-.3)<1e-6,
        "Subsequent exit must wait for the previous non-interruptible transition");
  auto malformed=timeline;
  malformed["timeline"].push_back({{"from","settled"},{"to","enter"},{"afterSeconds",0}});
  bool rejectedCycle=false;
  try{(void)AnimationStateMachine::parse(malformed);}catch(const std::invalid_argument&){rejectedCycle=true;}
  check(rejectedCycle,"Automatic transitions must form finite chains");
  malformed=timeline;
  malformed["timeline"][0]["afterSeconds"]=-.1;
  bool rejectedTime=false;
  try{(void)AnimationStateMachine::parse(malformed);}catch(const std::invalid_argument&){rejectedTime=true;}
  check(rejectedTime,"Negative timed transition boundaries must be rejected");
  const auto routed=nlohmann::json::parse(R"({
    "schemaVersion":4,"initial":"enter",
    "states":[{"name":"enter","clip":0},{"name":"loop","clip":1},{"name":"accent","clip":2},
              {"name":"leave","clip":3},{"name":"idle","clip":4}],
    "signals":["gesture","stop"],
    "transitions":[],
    "timeline":[{"from":"enter","to":"loop","afterSeconds":0.4,"blendSeconds":0.25,"startSeconds":0.1},
                {"from":"accent","to":"loop","afterSeconds":0.15,"blendSeconds":0.1},
                {"from":"leave","to":"idle","afterSeconds":0.3,"blendSeconds":0.05}],
    "branches":[{"from":"loop","to":"accent","signals":["gesture"],"once":true,"blendSeconds":0.1},
                {"from":"loop","to":"leave","signals":["stop"],"blendSeconds":0.2}]})");
  auto signals=AnimationStateMachine::parse(routed);
  signals.advance(.3);
  signals.select("gesture");
  signals.advance(.2);
  check(signals.state()=="loop"&&std::abs(signals.time()-.2)<1e-9&&std::abs(signals.blend()-.4)<1e-6,
        "A signal must wait for the authored entry and retain its destination offset");
  signals.advance(.16);
  check(signals.state()=="accent"&&signals.previousClip()==1&&std::abs(signals.blend()-.1)<1e-6,
        "A queued signal must blend from the completed pose using only remaining frame time");
  signals.advance(.3);
  check(signals.state()=="loop"&&signals.blend()==1,"A consumed signal must not retrigger when the loop resumes");
  signals.select("stop");
  signals.advance(.05);
  check(signals.state()=="leave"&&signals.previousClip()==1&&std::abs(signals.blend()-.25)<1e-6,
        "Changing the signal must run the authored return before choosing the next state");
  signals.advance(.31);
  check(signals.state()=="idle"&&signals.previousClip()==3&&signals.blend()==1,"The return must exit at its authored boundary");
  auto badSignal=routed;
  badSignal["branches"][0]["signals"][0]="unknown";
  bool rejectedSignal=false;
  try{(void)AnimationStateMachine::parse(badSignal);}catch(const std::invalid_argument&){rejectedSignal=true;}
  check(rejectedSignal,"Branch signals must be declared by the animation graph");
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
