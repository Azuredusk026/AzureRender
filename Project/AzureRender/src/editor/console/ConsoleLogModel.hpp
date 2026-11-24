#pragma once
#include "diagnostics/RuntimeDiagnostics.hpp"
#include <functional>
#include <string_view>
namespace azurerender {
struct ConsoleLogEntry { DiagnosticLevel level=DiagnosticLevel::Info;std::string text; };
DiagnosticLevel consoleMessageLevel(std::string_view message);
class ConsoleLogModel {
public:
    void update(const std::vector<std::string>& messages);
    void setPaused(bool value){paused_=value;}
    bool paused() const{return paused_;}
    const std::vector<ConsoleLogEntry>& entries() const{return entries_;}
    std::vector<std::size_t> visible(DiagnosticLevel minimum,const std::function<bool(const std::string&)>& search) const;
    std::string copyVisible(const std::vector<std::size_t>& rows) const;
private:
    bool paused_=false;
    std::vector<ConsoleLogEntry> entries_;
};
}
