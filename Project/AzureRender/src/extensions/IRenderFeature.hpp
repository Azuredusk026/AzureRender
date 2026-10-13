#pragma once

#include <string_view>
#include <string>
#include <vector>

namespace azurerender {

class IRenderFeature {
public:
    virtual ~IRenderFeature() = default;
    [[nodiscard]] virtual std::string_view name() const noexcept = 0;
    [[nodiscard]] virtual std::vector<std::string> capabilities() const {return {};}
    [[nodiscard]] virtual std::vector<std::string> resourceDependencies() const {return {};}
};

}  // namespace azurerender
