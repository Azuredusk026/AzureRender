#pragma once
#include <string>
#include <map>
namespace azurerender {
struct EditorInputEvent {
    std::string key;
    bool pressed=true, ctrl=false, text=false, modal=false;
    bool scene=false, viewport=false, navigating=false, playing=false, building=false;
};
struct EditorInputResult { bool consumed=false; std::string operation; int mode=-1; };
class EditorInputRouter {
public:
    static EditorInputResult route(const EditorInputEvent& event,const std::map<std::string,std::string>& bindings={});
};
}
