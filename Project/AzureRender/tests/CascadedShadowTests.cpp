#include "render/CascadedShadow.hpp"

#include <cassert>

int main() {
    const auto splits = azurerender::computeCascadeSplits(0.1F, 100.0F, 4, 0.65F);
    assert(splits.size() == 4);
    assert(splits.front() > 0.1F);
    assert(splits.back() == 100.0F);
    for (std::size_t i = 1; i < splits.size(); ++i) assert(splits[i] > splits[i - 1]);
    return 0;
}
