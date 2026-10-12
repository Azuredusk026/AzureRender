#pragma once
#include "ProjectCenterController.hpp"
namespace azurerender {
class ProjectCenterView final {
public:
    void draw(ProjectCenterController& controller);
private:
    std::string projectPath_, parent_, name_="Monica", diagnostic_;
    int template_=1;
};
}
