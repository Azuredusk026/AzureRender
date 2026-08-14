#pragma once

#include <string_view>

namespace azurerender {

class IRenderFeature {
public:
    virtual ~IRenderFeature() = default;
    [[nodiscard]] virtual std::string_view name() const noexcept = 0;
};

}  // namespace azurerender
