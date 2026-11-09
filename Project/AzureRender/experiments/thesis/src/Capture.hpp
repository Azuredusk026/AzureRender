#pragma once
#include <string>
namespace thesis {
class Capture {
public:
    Capture(const std::string& library,const std::string& prefix);
    ~Capture();
    void begin();
    void end();
private:
    void* api{};
    void* module{};
    bool active=false;
};
}
