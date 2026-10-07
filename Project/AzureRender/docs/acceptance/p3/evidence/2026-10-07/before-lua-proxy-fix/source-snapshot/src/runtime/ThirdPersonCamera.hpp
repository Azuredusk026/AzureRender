#pragma once
#include "runtime/GameComponents.hpp"
#include <algorithm>
#include <cmath>
#include <functional>
#include <stdexcept>
namespace azurerender {
class ThirdPersonCamera {
public:
    using Vector = std::array<float,3>;
    using Sweep = std::function<float(Vector,Vector,float)>;
    void reset(Vector target, const game::ThirdPersonCamera& settings) {
        validate(settings); target_ = target; target_[1] += settings.targetHeight;
        yaw_ = 0; pitch_ = std::clamp(15.0F,settings.minimumPitch,settings.maximumPitch);
        distance_ = actual_ = settings.distance; ready_ = true; place(settings);
    }
    void orbit(float x, float y, const game::ThirdPersonCamera& settings) {
        validate(settings);
        yaw_ = std::remainder(yaw_ + x*settings.sensitivity,360.0F);
        pitch_ = std::clamp(pitch_ + y*settings.sensitivity,settings.minimumPitch,settings.maximumPitch);
    }
    void zoom(float amount, const game::ThirdPersonCamera& settings) {
        validate(settings); distance_ = std::clamp(distance_-amount*.5F,settings.minimumDistance,settings.maximumDistance);
    }
    void update(Vector target, const game::ThirdPersonCamera& settings, float dt, const Sweep& sweep) {
        validate(settings); if (!std::isfinite(dt)||dt<0) throw std::invalid_argument("Invalid camera delta");
        if (!ready_) reset(target,settings);
        const float response=1-std::exp(-settings.response*dt); target[1]+=settings.targetHeight;
        for(unsigned i=0;i<3;++i)target_[i]+=(target[i]-target_[i])*response;
        const auto offset=direction(settings);
        const float fraction=std::clamp(sweep(target_,offset,settings.collisionRadius),0.0F,1.0F);
        const float allowed=std::max(.05F,distance_*fraction-.03F);
        actual_=allowed<actual_?allowed:actual_+(allowed-actual_)*response;
        place(settings);
    }
    float yaw() const { return yaw_; } float pitch() const { return pitch_; }
    float distance() const { return distance_; } float actualDistance() const { return actual_; }
    const Vector& position() const { return position_; } const Vector& target() const { return target_; }
private:
    static void validate(const game::ThirdPersonCamera& s) {
        if(s.minimumDistance>s.maximumDistance||s.distance<s.minimumDistance||s.distance>s.maximumDistance
            ||s.minimumPitch>s.maximumPitch||s.collisionRadius<=0||s.response<=0)throw std::invalid_argument("Invalid third-person camera settings");
    }
    Vector direction(const game::ThirdPersonCamera& settings) const {
        constexpr float radians=.017453292519943295F;const float y=yaw_*radians,p=pitch_*radians;
        return {-std::sin(y)*std::cos(p)*distance_+std::cos(y)*settings.shoulder,
                std::sin(p)*distance_,std::cos(y)*std::cos(p)*distance_+std::sin(y)*settings.shoulder};
    }
    void place(const game::ThirdPersonCamera& settings) {
        const auto offset=direction(settings);
        for(unsigned i=0;i<3;++i)position_[i]=target_[i]+offset[i]*(actual_/distance_);
    }
    Vector position_{},target_{};
    float yaw_=0,pitch_=15,distance_=4,actual_=4; bool ready_=false;
};
}
