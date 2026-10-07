#pragma once
#include <functional>
#include <map>
#include <string>
#include <thread>
#include <variant>
#include <vector>
#include <nlohmann/json.hpp>
namespace azurerender {
using ObservationValue=std::variant<bool,std::int64_t,double,std::string>;
// Readers belong to the host thread. Transports enqueue requests.
class ObservationRegistry {
public:
    using Reader=std::function<ObservationValue()>;
    void add(std::string name,Reader reader);
    std::vector<std::string> names() const;
    nlohmann::json query(const std::string& name) const;
private:
    std::map<std::string,Reader> readers_;
    std::thread::id owner_=std::this_thread::get_id();
    void checkThread() const;
};
}
