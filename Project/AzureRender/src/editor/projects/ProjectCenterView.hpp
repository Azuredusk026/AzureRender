#pragma once
#include "ProjectCenterController.hpp"
#include <functional>
namespace azurerender {
class ProjectCenterView final {
public:
    void draw(ProjectCenterController& controller,const std::function<void(const std::string&)>& observe={});
private:
    std::string projectPath_, parent_, name_="NewProject", diagnostic_;
    int template_=1;
};
}
