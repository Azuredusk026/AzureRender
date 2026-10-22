#pragma once
namespace azurerender {
struct LightReceivers {
    static constexpr unsigned kSchemaVersion=1;
    bool surfaces=true,characters=true;
    [[nodiscard]] unsigned mask() const noexcept {
        return (surfaces?1U:0U)|(characters?2U:0U);
    }
};
}
