#pragma once
#include "reflection/Annotations.hpp"
#include <cstdint>
namespace azurerender::test {
AZURE_TYPE("tool.metric", 1)
struct Metric {
    AZURE_FIELD("Samples", 0, 100)
    AZURE_FIELD_META("Statistics", "Collected sample count", true, false)
    std::uint32_t samples = 0;
};
}
