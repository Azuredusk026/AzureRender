#include "EditorFeedback.hpp"
#include <algorithm>
namespace azurerender {
std::string EditorFeedback::record(std::string source,std::string message) {
    if(!entries_.empty()&&entries_.back().source==source&&entries_.back().message==message)return entries_.back().id;
    const auto id="feedback-"+std::to_string(++sequence_);latest_=message;
    entries_.push_back({id,std::move(source),std::move(message)});return id;
}
void EditorFeedback::dismiss(const std::string& id) {
    if(id.empty())entries_.clear();
    else entries_.erase(std::remove_if(entries_.begin(),entries_.end(),[&](const auto& entry){return entry.id==id;}),entries_.end());
    latest_=entries_.empty()?std::string():entries_.back().message;
}
nlohmann::json EditorFeedback::report() const {
    auto result=nlohmann::json::array();for(const auto& entry:entries_)result.push_back({{"id",entry.id},{"source",entry.source},{"message",entry.message},{"severity","error"}});return result;
}
}
