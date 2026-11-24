#pragma once
#include <nlohmann/json.hpp>
#include <deque>
#include <string>
namespace azurerender {
struct StructuredDiagnostic {
    std::string id,source,code,severity="error",message,document,node,asset,path;
    unsigned line=0,occurrences=1;
    nlohmann::json describe() const{return {{"id",id},{"source",source},{"code",code},{"severity",severity},{"message",message},{"document",document},{"node",node},{"asset",asset},{"path",path},{"line",line},{"occurrences",occurrences}};}
};
class DiagnosticStore {
public:
    static constexpr std::size_t capacity=512;
    std::string record(StructuredDiagnostic diagnostic){
        if(diagnostic.source.empty()||diagnostic.code.empty()||diagnostic.message.empty()||(diagnostic.severity!="error"&&diagnostic.severity!="warning"&&diagnostic.severity!="info"))throw std::invalid_argument("Diagnostic requires source, code, severity and message");
        diagnostic.message=diagnostic.message.substr(0,16384);
        if(!entries_.empty()){
            auto previous=entries_.back().describe(),candidate=diagnostic.describe();for(auto* value:{&previous,&candidate}){value->erase("id");value->erase("occurrences");}
            if(previous==candidate){++entries_.back().occurrences;return entries_.back().id;}
        }
        diagnostic.id="problem-"+std::to_string(++sequence_);const auto id=diagnostic.id;
        if(entries_.size()==capacity){entries_.pop_front();++dropped_;}entries_.push_back(std::move(diagnostic));return id;
    }
    const StructuredDiagnostic& at(const std::string& id) const{for(const auto& entry:entries_)if(entry.id==id)return entry;throw std::invalid_argument("Unknown diagnostic identity");}
    nlohmann::json query(const nlohmann::json& filter=nlohmann::json::object()) const{
        auto result=nlohmann::json::array();for(const auto& entry:entries_){const auto row=entry.describe();bool accepted=true;
            for(const auto& field:filter.items()){if(field.key()=="text"){if((entry.message+" "+entry.source+" "+entry.code).find(field.value().get<std::string>())==std::string::npos)accepted=false;}
                else if(!row.contains(field.key())||row.at(field.key())!=field.value())accepted=false;}
            if(accepted)result.push_back(row);
        }return {{"capacity",capacity},{"dropped",dropped_},{"entries",result}};
    }
private:
    std::deque<StructuredDiagnostic> entries_;
    std::uint64_t sequence_=0,dropped_=0;
};
}
