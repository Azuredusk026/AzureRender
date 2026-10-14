#include "EditorToolbar.hpp"
#include "shell/EditorChrome.hpp"
namespace azurerender {
bool EditorToolbar::enabled(EditorSession& s,EditorCommand c){return EditorChrome::enabled(s,c);}
void EditorToolbar::draw(EditorSession& s,EditorWorkspace& w,float dpi,const Observer& o){EditorChrome::draw(s,w,dpi,o);}
void EditorToolbar::status(EditorSession& s,float dpi){EditorChrome::status(s,dpi);}
}
