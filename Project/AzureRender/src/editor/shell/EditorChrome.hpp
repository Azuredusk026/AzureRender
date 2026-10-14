#pragma once
#include "editor/EditorSession.hpp"
#include "editor/EditorWorkspace.hpp"
#include <functional>
namespace azurerender {
class EditorChrome {
public:
    using Observer=std::function<void(const std::string&)>;
    static bool enabled(EditorSession&,EditorCommand);
    static void draw(EditorSession&,EditorWorkspace&,float,const Observer&);
    static void status(EditorSession&,float);
};
}
