#include "EditorInputRouter.hpp"
namespace azurerender {
EditorInputResult EditorInputRouter::route(const EditorInputEvent& input,const std::map<std::string,std::string>& bindings) {
    auto e=input;
    if(!e.ctrl && !bindings.empty()) {
        bool configured=false;
        for(const auto& binding:bindings)if(binding.second==input.key){e.key=binding.first;configured=true;break;}
        if(!configured && bindings.count(input.key))e.key.clear();
    }
    if(!e.pressed || e.text || e.modal)return {};
    if(e.ctrl) {
        if(e.key=="P")return {true,e.playing?"preview.stop":"preview.play",-1};
        if(e.playing || e.building)return {};
        if(e.key=="S")return {true,"document.save",-1};
        if(e.key=="Z")return {true,"history.undo",-1};
        if(e.key=="Y")return {true,"history.redo",-1};
        if(e.scene && e.key=="D")return {true,"node.duplicate",-1};
        return {};
    }
    if(e.playing || e.building || e.navigating)return {};
    if(e.scene && e.key=="Delete")return {true,"node.delete",-1};
    if(e.scene && e.key=="F")return {true,"viewport.frame-selection",-1};
    if(e.viewport) {
        if(e.key=="W")return {true,"viewport.gizmo-mode",0};
        if(e.key=="E")return {true,"viewport.gizmo-mode",1};
        if(e.key=="R")return {true,"viewport.gizmo-mode",2};
        if(e.key=="Q")return {true,"viewport.gizmo-mode",3};
    }
    return {};
}
}
