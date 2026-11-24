#pragma once
#include "EditorSession.hpp"
#include "EditorWorkspace.hpp"
#include <functional>
namespace azurerender {
class EditorToolbar {
public:
    using Observer=std::function<void(const std::string&)>;
    static bool enabled(EditorSession&,EditorCommand);
    static void draw(EditorSession&,EditorWorkspace&,float,const Observer&);
    static void status(EditorSession&,float);
};
}
