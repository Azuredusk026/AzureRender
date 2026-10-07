#include "editor/input/EditorInputRouter.hpp"
#include <iostream>
int main(){
    using namespace azurerender;
    EditorInputEvent event;event.key="W";event.scene=true;event.viewport=true;
    auto tool=EditorInputRouter::route(event);
    if(tool.operation!="viewport.gizmo-mode" || tool.mode!=0){std::cerr<<"W must select move\n";return 1;}
    event.key="E";if(EditorInputRouter::route(event).mode!=1)return 2;
    event.key="R";if(EditorInputRouter::route(event).mode!=2)return 3;
    event.text=true;if(EditorInputRouter::route(event).consumed)return 4;
    event.text=false;event.modal=true;if(EditorInputRouter::route(event).consumed)return 5;
    event.modal=false;event.navigating=true;if(EditorInputRouter::route(event).operation!="")return 6;
    event.navigating=false;event.key="F";if(EditorInputRouter::route(event).operation!="viewport.frame-selection")return 7;
    event.key="Delete";event.scene=false;if(EditorInputRouter::route(event).consumed)return 8;
    event.key="S";event.ctrl=true;if(EditorInputRouter::route(event).operation!="document.save")return 9;
    event.ctrl=false;event.viewport=true;event.scene=true;event.key="G";
    if(EditorInputRouter::route(event,{{"W","G"}}).mode!=0){std::cerr<<"Rebound move key must route through the shared operation\n";return 10;}
    event.key="W";if(EditorInputRouter::route(event,{{"W","G"}}).consumed)return 11;
    return 0;
}
