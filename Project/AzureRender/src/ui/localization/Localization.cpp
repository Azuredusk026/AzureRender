#include "ui/localization/Localization.hpp"
#include <array>
#include <map>
#include <string>
#include <stdexcept>
namespace azurerender::ui {
namespace {
Language current=Language::Chinese;
using Entry=std::array<std::string,2>;
std::map<std::string,Entry>& catalog(){
    static std::map<std::string,Entry> values=[] {
        const char* pairs[][2]={
#include "Catalog.inc"
        };
        std::map<std::string,Entry> result;
        for(const auto& pair:pairs){Entry entry{pair[0],pair[1]};result.emplace(pair[0],entry);result.emplace(pair[1],entry);}
        return result;
    }();return values;
}
}
void setLanguage(Language value){if(value!=Language::Chinese&&value!=Language::English)throw std::invalid_argument("Unknown UI language");current=value;}
Language language(){return current;}
const char* tr(const char* text){
    if(!text)return text;
    auto& values=catalog();auto found=values.find(text);
    if(found==values.end()){
        const std::string source(text);const auto suffix=source.find("##");
        if(suffix!=std::string::npos){
            const auto base=values.find(source.substr(0,suffix));
            if(base!=values.end()){
                Entry entry{base->second[0]+source.substr(suffix),base->second[1]+source.substr(suffix)};
                found=values.emplace(source,std::move(entry)).first;
            }
        }
    }
    return found==values.end()?text:found->second[static_cast<unsigned>(current)].c_str();
}
}
