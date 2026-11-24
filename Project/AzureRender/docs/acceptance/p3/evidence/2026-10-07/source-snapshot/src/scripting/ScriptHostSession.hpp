#pragma once
#include "scripting/ScriptInterop.hpp"
#include "scripting/ScriptBindingHost.hpp"
#include <memory>
namespace azurerender {
// A saved ABI table owns no raw host pointer. Closing its token expires it.
class ScriptHostSession final {
public:
    explicit ScriptHostSession(std::shared_ptr<ScriptBindingHost>);
    ~ScriptHostSession(){close();}
    ScriptHostSession(const ScriptHostSession&)=delete;
    ScriptHostSession& operator=(const ScriptHostSession&)=delete;
    ScriptHostApi api() const noexcept {return api_;}
    void close() noexcept;
private:
    ScriptHostApi api_;
};
}
