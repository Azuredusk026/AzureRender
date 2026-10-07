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
private:const EditorContext& context_;EditService& edits_;
};
}
