#pragma once
#include <array>
namespace azurerender::ui {
struct ViewportDesignTokens {
    static constexpr std::array<float,4> minorGrid{.40F,.45F,.52F,.16F};
    static constexpr std::array<float,4> majorGrid{.56F,.61F,.68F,.38F};
    static constexpr std::array<float,4> xAxis{.70F,.18F,.18F,.80F};
    static constexpr std::array<float,4> zAxis{.18F,.36F,.70F,.80F};
};
}
