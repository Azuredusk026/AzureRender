#pragma once
#include <string>
#include <vector>
namespace azurerender {
class EditorContext;class EditService;
class SelectionService {
public:
    SelectionService(const EditorContext& context,EditService& edits):context_(context),edits_(edits){}
    void set(const std::vector<std::string>& identities);
    std::vector<std::string> selected() const;
    void click(const std::string& identity,bool ctrl,bool shift,const std::vector<std::string>& visible);
    std::string active() const;
    const std::string& anchor() const noexcept{return anchor_;}
    bool consumeReveal(){const bool result=reveal_;reveal_=false;return result;}
private:const EditorContext& context_;EditService& edits_;
    std::string anchor_;
    bool reveal_=false;
};
}
