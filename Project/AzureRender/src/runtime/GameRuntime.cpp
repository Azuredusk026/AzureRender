#include "runtime/GameRuntime.hpp"
#include "runtime/GameComponents.hpp"
#include <chrono>
namespace azurerender {
double GameRuntime::advance(double delta) {
    if (!std::isfinite(delta) || delta < 0) throw std::invalid_argument("Invalid game frame delta");
    constexpr double fixed = 1.0 / 60.0;
    if (runtime_.state() == RuntimeLifecycle::State::Paused) {
        accumulator_ = 0;
        if (!runtime_.stepPending()) return 0;
        delta = fixed;
    }
    if (runtime_.state() != RuntimeLifecycle::State::Running && runtime_.state() != RuntimeLifecycle::State::Paused) return 0;
    accumulator_ += std::min(delta, 0.25);
    double simulated = 0;
    for (unsigned count = 0; accumulator_ + 1e-12 >= fixed && count < 15; ++count) {
        const auto start = std::chrono::steady_clock::now();
        const auto elapsed = runtime_.beginFrame(fixed);
        if (!elapsed) break;
        motions_.clear();
        if (beforeStep_) beforeStep_(fixed);
        std::map<ecs::Entity, CharacterMotion> motions;
        runtime_.world().each<game::Character>([&](auto entity, auto&) {
            motions[entity] = {static_cast<float>(input_.down("move-right")) - static_cast<float>(input_.down("move-left")),
                static_cast<float>(input_.down("move-back")) - static_cast<float>(input_.down("move-forward")), input_.pressed("jump")};
        });
        for (const auto& motion : motions_) motions[motion.first] = motion.second;
        const auto events = physics_.step(runtime_, static_cast<float>(fixed), motions);
        for (const auto& event : events) if (eventHandler_) eventHandler_(event);
        input_.endStep(); accumulator_ -= fixed; simulated += fixed; ++steps_;
        simulationMilliseconds_ += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    }
    return simulated;
}
}
