#include "ConsoleLogModel.hpp"
namespace azurerender {
DiagnosticLevel consoleMessageLevel(std::string_view message){
    if(message.empty()||message.front()!='[')return DiagnosticLevel::Info;
    const auto timeEnd=message.find(']');
    if(timeEnd==std::string_view::npos||timeEnd<=1)return DiagnosticLevel::Info;
    for(std::size_t index=1;index<timeEnd;++index)if(message[index]<'0'||message[index]>'9')return DiagnosticLevel::Info;
    if(message.substr(timeEnd,10)=="][warning]")return DiagnosticLevel::Warning;
    if(message.substr(timeEnd,8)=="][error]")return DiagnosticLevel::Error;
    return DiagnosticLevel::Info;
}
void ConsoleLogModel::update(const std::vector<std::string>& messages){
    if(paused_)return;
    entries_.clear();entries_.reserve(messages.size());
    for(const auto& message:messages)entries_.push_back({consoleMessageLevel(message),message});
}
std::vector<std::size_t> ConsoleLogModel::visible(DiagnosticLevel minimum,const std::function<bool(const std::string&)>& search) const{
    std::vector<std::size_t> result;
    for(std::size_t index=0;index<entries_.size();++index)
        if(entries_[index].level>=minimum&&(!search||search(entries_[index].text)))result.push_back(index);
    return result;
}
std::string ConsoleLogModel::copyVisible(const std::vector<std::size_t>& rows) const{
    std::string result;for(const auto index:rows)result+=entries_.at(index).text+'\n';return result;
}
}
