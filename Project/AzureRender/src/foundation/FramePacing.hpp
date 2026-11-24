#pragma once

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace azurerender {

// Pure clock-independent policy shared by hosts. The caller owns the timer and
// performs the wait after frame work; neither rendering nor simulation is skipped.
inline double framePacingWaitSeconds(double elapsedSeconds, unsigned maximumRate) {
    if (!std::isfinite(elapsedSeconds) || elapsedSeconds < 0 || maximumRate > 1000)
        throw std::invalid_argument("Invalid frame pacing time or rate");
    return maximumRate == 0 ? 0 : std::max(0.0, 1.0 / maximumRate - elapsedSeconds);
}

} // namespace azurerender
