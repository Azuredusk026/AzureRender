#include "runtime/ObservationRegistry.hpp"
#include <cmath>
#include <regex>
#include <stdexcept>
namespace azurerender {
void ObservationRegistry::checkThread() const {
    if(std::this_thread::get_id()!=owner_)throw std::logic_error("Observation requires its owner thread");
}
void ObservationRegistry::add(std::string name,Reader reader) {
    checkThread();
    if(name.size()>128||!std::regex_match(name,std::regex("[A-Za-z][A-Za-z0-9_.-]*"))||!reader)
        throw std::invalid_argument("Invalid observation registration");
    if(!readers_.emplace(std::move(name),std::move(reader)).second)throw std::invalid_argument("Duplicate observation");
}
std::vector<std::string> ObservationRegistry::names() const {
    checkThread();std::vector<std::string> result;for(const auto& item:readers_)result.push_back(item.first);return result;
}
nlohmann::json ObservationRegistry::query(const std::string& name) const {
    checkThread();const auto found=readers_.find(name);
    if(found==readers_.end())throw std::invalid_argument("Unknown observation: "+name);
    const auto value=found->second();
    if(const auto* number=std::get_if<double>(&value);number&&!std::isfinite(*number))throw std::invalid_argument("Non-finite observation");
    if(const auto* text=std::get_if<std::string>(&value);text&&text->size()>65536)throw std::invalid_argument("Observation text exceeds budget");
    return std::visit([](const auto& item){return nlohmann::json(item);},value);
}
}
