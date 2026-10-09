#pragma once
#include "editor/EditorContext.hpp"
#include "EditorViewDescriptor.hpp"
#include <functional>
#include <map>
namespace azurerender {
struct ViewportOverlayLine {std::array<float,3> from,to;std::array<float,4> color;std::string node;};
struct ViewportOverlayRequest {const EditorContext& context;const EditorViewDescriptor& view;};
class ViewportOverlayRegistry {
public:
    using Provider=std::function<std::vector<ViewportOverlayLine>(const ViewportOverlayRequest&)>;
    void add(std::string category,std::string label,Provider);
    nlohmann::json describe() const;
    std::vector<ViewportOverlayLine> collect(const ViewportOverlayRequest&,const std::map<std::string,std::string>& policies,const std::vector<std::string>& selected) const;
    bool contains(const std::string& category) const{return providers_.count(category)!=0;}
    static ViewportOverlayRegistry builtins();
private:
    struct Entry {std::string label;Provider provider;};
    std::map<std::string,Entry> providers_;
};
}
