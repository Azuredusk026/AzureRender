#include "SettingRegistry.hpp"
#include <cmath>
#include <fstream>
#include <stdexcept>
namespace azurerender {
namespace {
std::size_t index(SettingSource source) {
    const auto value=static_cast<std::size_t>(source);
    if(value>=6)throw std::invalid_argument("Unknown setting source");
    return value;
}
std::pair<nlohmann::json,SettingSource> resolve(const std::array<std::optional<nlohmann::json>,6>& values) {
    for(std::size_t i=6;i>0;--i)if(values[i-1])return {*values[i-1],static_cast<SettingSource>(i-1)};
    throw std::logic_error("Setting default missing");
}
}
const char* settingSourceName(SettingSource source) {
    static const char* names[]={"default","default-file","user-file","project","command-line","console"};
    return names[index(source)];
}
void SettingRegistry::checkThread() const { if(owner_!=std::this_thread::get_id())throw std::logic_error("Settings require owner thread"); }
void SettingRegistry::validate(const Entry& entry,const Json& value,SettingSource source) const {
    index(source);
    const auto& d=entry.descriptor;const auto& base=d.defaultValue;
    if(source!=SettingSource::Default && (d.readOnly || (d.startupOnly&&started_)))throw std::invalid_argument("Setting is immutable in this lifecycle: "+d.name);
    const bool match=base.is_boolean()?value.is_boolean():base.is_string()?value.is_string():base.is_number_integer()?value.is_number_integer():base.is_number_float()?value.is_number():false;
    if(!match)throw std::invalid_argument("Setting type mismatch: "+d.name);
    if(value.is_number()) { const auto v=value.get<double>();if(!std::isfinite(v)||(d.minimum&&v<*d.minimum)||(d.maximum&&v>*d.maximum))throw std::invalid_argument("Setting outside range: "+d.name); }
    if(value.is_string()&&value.get_ref<const std::string&>().size()>65536)throw std::invalid_argument("Setting text exceeds budget");
}
void SettingRegistry::add(SettingDescriptor d) {
    checkThread();if(started_||d.name.empty()||d.name.size()>256||entries_.count(d.name)||entries_.size()>=1024)throw std::invalid_argument("Invalid setting registration");
    if((d.minimum&&!std::isfinite(*d.minimum))||(d.maximum&&!std::isfinite(*d.maximum))||(d.minimum&&d.maximum&&*d.minimum>*d.maximum))throw std::invalid_argument("Invalid setting bounds");
    Entry entry;entry.descriptor=std::move(d);validate(entry,entry.descriptor.defaultValue,SettingSource::Default);
    entry.layers[0]=entry.descriptor.defaultValue;entry.pending=entry.layers;entries_.emplace(entry.descriptor.name,std::move(entry));
}
SettingResult SettingRegistry::set(const std::string& name,Json value,SettingSource source) {
    checkThread();try { if(source==SettingSource::Default)throw std::invalid_argument("Default belongs to declaration");auto& e=entries_.at(name);validate(e,value,source);e.pending[index(source)]=std::move(value);return {true,{}}; }
    catch(const std::exception& error){return {false,error.what()};}
}
SettingResult SettingRegistry::reset(const std::string& name,SettingSource source) {
    checkThread();try {if(source==SettingSource::Default)throw std::invalid_argument("Default belongs to declaration");auto& e=entries_.at(name);validate(e,e.descriptor.defaultValue,source);e.pending[index(source)].reset();return {true,{}};}
    catch(const std::exception& error){return {false,error.what()};}
}
SettingResult SettingRegistry::replaceLayer(const Json& values,SettingSource source) {
    checkThread();try {
        if(source==SettingSource::Default||!values.is_object()||values.dump().size()>2*1024*1024)throw std::invalid_argument("Invalid setting layer");
        auto candidate=entries_;
        for(const auto& field:values.items())validate(candidate.at(field.key()),field.value(),source);
        for(auto& [name,e]:candidate) {
            if(e.pending[index(source)]&&!values.contains(name))validate(e,e.descriptor.defaultValue,source);
            e.pending[index(source)].reset();
        }
        for(const auto& field:values.items())candidate.at(field.key()).pending[index(source)]=field.value();
        entries_.swap(candidate);return {true,{}};
    }catch(const std::exception& error){return {false,error.what()};}
}
void SettingRegistry::applyPending() {checkThread();bool changed=false;for(auto& [name,e]:entries_)if(e.layers!=e.pending){e.layers=e.pending;changed=true;}if(changed)++revision_;}
void SettingRegistry::start() {checkThread();applyPending();started_=true;}
SettingRegistry::Json SettingRegistry::get(const std::string& name) const {checkThread();return resolve(entries_.at(name).layers).first;}
SettingSource SettingRegistry::source(const std::string& name) const {checkThread();return resolve(entries_.at(name).layers).second;}
SettingRegistry::Json SettingRegistry::describe(const std::string& search) const {
    checkThread();Json result=Json::object();for(const auto& [name,e]:entries_) {
        if(name.find(search)==std::string::npos&&e.descriptor.description.find(search)==std::string::npos)continue;
        const auto [value,source]=resolve(e.layers);const auto [pending,pendingSource]=resolve(e.pending);const auto& d=e.descriptor;
        Json layers=Json::object();for(std::size_t i=0;i<6;++i)if(e.layers[i])layers[settingSourceName(static_cast<SettingSource>(i))]=*e.layers[i];
        result[name]={{"type",d.defaultValue.is_boolean()?"boolean":d.defaultValue.is_string()?"string":d.defaultValue.is_number_integer()?"integer":"number"},{"description",d.description},{"value",value},{"default",d.defaultValue},{"source",settingSourceName(source)},
            {"pending",pending},{"pendingSource",settingSourceName(pendingSource)},{"layers",layers},{"readOnly",d.readOnly},{"persistent",d.persistent},{"startupOnly",d.startupOnly}};
        if(d.minimum)result[name]["minimum"]=*d.minimum;if(d.maximum)result[name]["maximum"]=*d.maximum;
    }return result;
}
void SettingRegistry::saveUser(const std::filesystem::path& path) const {
    checkThread();Json values=Json::object();for(const auto& [name,e]:entries_)if(e.descriptor.persistent&&!e.descriptor.readOnly&&e.layers[2])values[name]=*e.layers[2];
    if(!path.parent_path().empty())std::filesystem::create_directories(path.parent_path());
    auto pending=path;pending+=".pending";
    {std::ofstream output(pending,std::ios::binary);output<<Json{{"schemaVersion",1},{"values",values}}.dump(2);if(!output)throw std::runtime_error("Settings save failed");}
    std::filesystem::copy_file(pending,path,std::filesystem::copy_options::overwrite_existing);std::filesystem::remove(pending);
}
SettingResult SettingRegistry::load(const std::filesystem::path& path,SettingSource source) {
    checkThread();try {
        if(!std::filesystem::exists(path))return {true,{}};
        if(std::filesystem::file_size(path)>2*1024*1024)throw std::invalid_argument("Settings file exceeds budget");
        std::ifstream input(path);const auto data=Json::parse(input);
        if(!data.is_object()||data.size()!=2||!data.at("schemaVersion").is_number_integer()||data.at("schemaVersion")!=1)throw std::invalid_argument("Unsupported settings schema");
        return replaceLayer(data.at("values"),source);
    }catch(const std::exception& error){return {false,error.what()};}
}
}
