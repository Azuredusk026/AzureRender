#include "editor/EditorSession.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
using namespace azurerender;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(int argc,char** argv){if(argc!=2)return 2;try{
    const auto root=std::filesystem::temp_directory_path()/("azure-animation-authoring-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::copy(argv[1],root,std::filesystem::copy_options::recursive);
    auto context=EditorContext::openProject(root/"project.azureproject");EditorSession session(context);
    const auto document=context->documentContent();
    require(static_cast<bool>(session.edit("animation.seek",{{"node","hero"},{"state","walk"},{"time",.25}})),"Seek a configured skeleton without altering its component");
    require(static_cast<bool>(session.edit("animation.seek",{{"node","guide"},{"state","idle"},{"time",.5}})),"Second skeleton has an independent timeline");
    require(static_cast<bool>(session.edit("animation.play",{{"node","hero"}})),"Play first skeleton");
    session.advance(.1);
    auto report=session.edit("animation.describe");require(static_cast<bool>(report),"Timeline can be queried");
    const auto tracks=report.value;
    require(tracks.at("hero").at("time").get<double>()>.34&&tracks.at("guide").at("time")==.5,"Only playing skeleton advances");
    require(static_cast<bool>(session.edit("node.select",{{"id","guide"}})),"Switch selected object");
    require(session.edit("animation.describe").value.at("hero").at("playing")==true,"Selection preserves other timeline");
    require(static_cast<bool>(session.edit("animation.pause",{{"node","hero"}})),"Pause first skeleton");session.advance(.2);
    require(session.edit("animation.describe").value.at("hero").at("time")==tracks.at("hero").at("time"),"Pause preserves exact time");
    require(!session.edit("animation.seek",{{"node","hero"},{"state","missing"},{"time",1}}),"Illegal state is rejected");
    require(session.edit("animation.describe").value.at("hero").at("time")==tracks.at("hero").at("time"),"Failed seek preserves track");
    require(!session.edit("animation.seek",{{"node","hero"},{"time",-1}}),"Negative time is rejected");
    require(context->documentContent()==document&&!context->dirty()&&context->undoCount()==0,"Animation controls are transient");
    require(static_cast<bool>(session.edit("node.select",{{"id","hero"}})),"Return to first object");
    require(session.edit("animation.describe").value.size()==2,"Switching restores both independent tracks");
    require(static_cast<bool>(session.edit("animation.clear",{{"node","guide"}})),"Clear one track");
    require(session.edit("animation.describe").value.size()==1,"Clear releases only the selected track");
    require(static_cast<bool>(session.edit("animation.configure",{{"node","hero"},{"speed",2.},{"loop",false}})),"Preview speed and loop belong to each timeline");
    require(static_cast<bool>(session.edit("animation.seek",{{"node","hero"},{"time",0}})),"Restart configured track");
    require(static_cast<bool>(session.edit("animation.play",{{"node","hero"}})),"Start accelerated preview");session.advance(.1);
    require(std::abs(session.edit("animation.describe").value.at("hero").at("time").get<double>()-.2)<1e-6,"Playback rate changes elapsed clip time");
    const auto lines=session.animations().skeletonLines(*context);require(!lines.empty(),"Skeleton preview uses actual sampled joints");
    for(const auto& line:lines)require(line.node=="hero","Skeleton helpers identify the owning timeline");
    session.advance(20);require(!session.edit("animation.describe").value.at("hero").at("playing").get<bool>(),"Nonloop preview stops at measured clip duration");
    require(!session.edit("animation.configure",{{"node","hero"},{"speed",0.},{"loop",true}}),"Zero playback speed rejects without changing track");
    require(context->documentContent()==document&&!context->dirty(),"Playback settings and skeleton helpers preserve authored content");
    context->assets().refresh();session.advance(0);
    require(session.edit("animation.describe").value.empty(),"Refreshing source assets invalidates stale skeleton tracks");
    require(static_cast<bool>(session.edit("animation.play",{{"node","hero"}})),"A refreshed binding creates a new preview track");
    require(static_cast<bool>(session.edit("component.field",{{"type","azure.animator"},{"field","state"},{"value","walk"}})),"Rebind initial state using validated component metadata");
    session.advance(0);require(session.edit("animation.describe").value.empty(),"Content edits invalidate preview bindings before rendering");
    std::filesystem::remove_all(root);std::cout<<"Independent skeleton timelines passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
