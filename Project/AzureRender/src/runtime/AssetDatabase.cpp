#include "runtime/AssetDatabase.hpp"
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <random>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
namespace azurerender {
namespace {
using Json = nlohmann::json;
std::string bytes(const std::filesystem::path& path, const std::function<void()>& check={}) {
    std::ifstream input(path, std::ios::binary|std::ios::ate);
    if (!input) throw std::runtime_error("Cannot read asset: " + path.string());
    const auto size=input.tellg();if(size<0)throw std::runtime_error("Cannot size asset: "+path.string());
    std::string data(static_cast<std::size_t>(size),'\0');input.seekg(0);
    for(std::size_t offset=0;offset<data.size();){
        if(check)check();const auto count=std::min<std::size_t>(1024*1024,data.size()-offset);
        input.read(data.data()+offset,static_cast<std::streamsize>(count));offset+=count;
    }
    if (!input) throw std::runtime_error("Asset read failed: " + path.string());
    return data;
}
std::uint64_t hash(const std::string& data) {
    std::uint64_t result = 14695981039346656037ULL;
    for (const unsigned char c : data) { result ^= c; result *= 1099511628211ULL; }
    return result;
}
void write(const std::filesystem::path& path, const std::string& data) {
    std::filesystem::create_directories(path.parent_path());
    const auto temporary = path.string() + ".tmp";
    { std::ofstream output(temporary, std::ios::binary); output << data; output.flush();
      if (!output) throw std::runtime_error("Cannot write asset file: " + path.string()); }
    std::filesystem::copy_file(temporary, path, std::filesystem::copy_options::overwrite_existing);
    std::filesystem::remove(temporary);
}
std::string uuid() {
    std::random_device random;
    std::ostringstream out; out << std::hex << std::setfill('0');
    for (int i = 0; i < 16; ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10) out << '-';
        unsigned value = random() & 255;
        if (i == 6) value = (value & 15) | 64;
        if (i == 8) value = (value & 63) | 128;
        out << std::setw(2) << value;
    }
    return out.str();
}
void append64(std::string& out, std::uint64_t value) {
    for (unsigned i = 0; i < 8; ++i) out.push_back(static_cast<char>((value >> (i * 8)) & 255));
}
std::uint64_t read64(const std::string& value, std::size_t offset) {
    std::uint64_t result = 0;
    for (unsigned i = 0; i < 8; ++i) result |= static_cast<std::uint64_t>(static_cast<unsigned char>(value[offset + i])) << (i * 8);
    return result;
}
void cache(const std::filesystem::path& path, const AssetRecord& record) {
    const auto source = bytes(record.path);
    if (hash(source) != record.contentHash) throw std::runtime_error("Asset changed during import: " + record.path.string());
    std::string data = "AZCACHE1"; append64(data, record.fingerprint); append64(data, source.size()); data += source;
    write(path, data);
}
}
std::vector<std::string> AssetDatabase::refresh(bool verifyAll,std::function<void()> check) {
    statistics_={};
    std::map<std::string, AssetRecord> candidate;
    std::map<std::string, Json> metadata;
    std::map<std::string, std::string> paths;
    std::map<std::filesystem::path,SourceState> sources;
    std::map<std::string,Json> documents;
    for (const auto& mount : project_.mounts) {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(mount.second)) {
            if(check)check();
            if (!entry.is_regular_file() || entry.path().extension() == ".azmeta" || entry.path().extension() == ".tmp") continue;
            const auto virtualPath = mount.first + ":/" + entry.path().lexically_relative(mount.second).generic_string();
            const auto path = project_.resolve(virtualPath);
            const auto sidecar = path.string() + ".azmeta";
            const auto previous=sources_.find(path);
            const auto time=std::filesystem::last_write_time(path);
            const auto size=std::filesystem::file_size(path);
            if(!verifyAll && previous!=sources_.end() && std::filesystem::exists(sidecar)
                && previous->second.sourceTime==time && previous->second.size==size
                && previous->second.metadataTime==std::filesystem::last_write_time(sidecar)){
                const auto& state=previous->second;
                if(!candidate.emplace(state.id,AssetRecord{state.id,virtualPath,path,state.baseFingerprint,state.contentHash,{}}).second)
                    throw std::runtime_error("Duplicate asset UUID: "+state.id);
                metadata[state.id]=state.metadata;documents[state.id]=state.document;paths[virtualPath]=state.id;
                sources[path]=state;++statistics_.filesReused;continue;
            }
            Json meta;
            if (std::filesystem::exists(sidecar)) meta = Json::parse(bytes(sidecar));
            else { meta = {{"schemaVersion", 1}, {"id", uuid()}, {"settings", Json::object()}, {"dependencies", Json::array()}}; write(sidecar, meta.dump(2)); }
            if (!meta.at("schemaVersion").is_number_integer() || meta.at("schemaVersion") != 1) throw std::runtime_error("Unsupported asset metadata version");
            const auto id = meta.at("id").get<std::string>();
            if (!std::regex_match(id, std::regex("[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}")))
                throw std::runtime_error("Invalid asset UUID: " + sidecar);
            const auto source = bytes(path,check);
            statistics_.sourceBytesRead+=source.size();++statistics_.filesRead;
            if (!candidate.emplace(id, AssetRecord{id, virtualPath, path, hash(source + meta.value("settings", Json::object()).dump() + "importer-v1"), hash(source), {}}).second)
                throw std::runtime_error("Duplicate asset UUID: " + id);
            const auto extension=path.extension().string();Json document;
            if(extension==".gltf" || extension==".azurelevel" || extension==".azureprefab")document=Json::parse(source);
            const auto& record=candidate.at(id);
            sources[path]={time,std::filesystem::last_write_time(sidecar),size,id,meta,document,record.contentHash,record.fingerprint};
            metadata[id] = meta; documents[id]=std::move(document); paths[virtualPath] = id;
        }
    }
    for (auto& entry : candidate) {
        if(check)check();
        auto& record = entry.second;
        std::set<std::string> dependencies;
        const auto add = [&](const std::string& reference) {
            if (reference.empty() || reference.rfind("engine:/", 0) == 0) return;
            auto id = reference;
            if (reference.find(":/") != std::string::npos) {
                const auto found = paths.find(reference);
                if (found == paths.end()) throw std::runtime_error("Missing virtual asset dependency: " + reference);
                id = found->second;
            }
            if (!candidate.count(id)) throw std::runtime_error("Missing asset dependency: " + id);
            dependencies.insert(id);
        };
        for (const auto& dep : metadata.at(record.id).value("dependencies", Json::array())) add(dep.get<std::string>());
        const auto extension = record.path.extension().string();
        if (extension == ".gltf" || extension == ".azurelevel" || extension == ".azureprefab") {
            const Json& document = documents.at(record.id);
            const auto walk = [&](const Json& value, auto&& self) -> void {
                if (value.is_object()) for (const auto& field : value.items()) {
                    if ((field.key() == "asset" || field.key() == "prefab") && field.value().is_string()) add(field.value().get<std::string>());
                    else self(field.value(), self);
                }
                else if (value.is_array()) for (const auto& child : value) self(child, self);
            };
            walk(document, walk);
            if (extension == ".gltf") for (const auto* key : {"buffers", "images"})
                for (const auto& object : document.value(key, Json::array())) {
                    const auto uri = object.value("uri", std::string());
                    if (uri.empty() || uri.rfind("data:", 0) == 0) continue;
                    const auto slash = record.virtualPath.find(":/");
                    const auto relative = (std::filesystem::path(record.virtualPath.substr(slash + 2)).parent_path() / uri).lexically_normal();
                    const auto virtualPath = record.virtualPath.substr(0, slash + 2) + relative.generic_string();
                    static_cast<void>(project_.resolve(virtualPath)); add(virtualPath);
                }
        }
        record.dependencies.assign(dependencies.begin(), dependencies.end());
    }
    std::map<std::string, unsigned> state;
    const auto fingerprint = [&](const std::string& id, auto&& self) -> std::uint64_t {
        if (state[id] == 1) throw std::runtime_error("Asset dependency cycle: " + id);
        auto& record = candidate.at(id);
        if (state[id] == 2) return record.fingerprint;
        state[id] = 1;
        std::string key = std::to_string(record.fingerprint);
        for (const auto& dependency : record.dependencies) key += ":" + std::to_string(self(dependency, self));
        record.fingerprint = hash(key); state[id] = 2; return record.fingerprint;
    };
    std::vector<std::string> changed;
    for (auto& entry : candidate) {
        fingerprint(entry.first, fingerprint);
        const auto old = records_.find(entry.first);
        if (old == records_.end() || old->second.fingerprint != entry.second.fingerprint || old->second.path != entry.second.path) {
            changed.push_back(entry.first); cache(project_.file.parent_path() / ".azure/cache" / entry.first / (std::to_string(entry.second.fingerprint) + ".azcache"), entry.second);
        }
    }
    for (const auto& old : records_) if (!candidate.count(old.first)) changed.push_back(old.first);
    if(check)check();
    records_.swap(candidate);
    sources_.swap(sources);
    return changed;
}
std::string AssetDatabase::idForPath(const std::string& path) const {
    for (const auto& entry : records_) if (entry.second.virtualPath == path) return entry.first;
    throw std::runtime_error("Unknown asset path: " + path);
}
std::filesystem::path AssetDatabase::resolve(const std::string& id) const {
    const auto found = records_.find(id);
    if (found == records_.end()) throw std::runtime_error("Unknown asset UUID: " + id);
    return found->second.path;
}
std::filesystem::path AssetDatabase::resolveReference(const std::string& reference) const {
    const auto result = reference.find(":/") == std::string::npos ? resolve(reference) : project_.resolve(reference);
    if (!std::filesystem::is_regular_file(result)) throw std::runtime_error("Asset reference missing: " + reference);
    return result;
}
std::filesystem::path AssetDatabase::cacheFile(const std::string& id) const {
    return project_.file.parent_path() / ".azure/cache" / id / (std::to_string(records_.at(id).fingerprint) + ".azcache");
}
std::string AssetDatabase::readSource(const std::string& id) const {
    const auto& record = records_.at(id);
    const auto path = cacheFile(id);
    if (std::filesystem::exists(path)) {
        const auto data = bytes(path);
        if (data.size() >= 24 && data.substr(0, 8) == "AZCACHE1" && read64(data, 8) == record.fingerprint
            && read64(data, 16) == data.size() - 24 && hash(data.substr(24)) == record.contentHash) return data.substr(24);
    }
    cache(path, record); return bytes(record.path);
}
void AssetDatabase::writePack(const std::filesystem::path& destination) const {
    if (std::filesystem::exists(destination) && !std::filesystem::is_empty(destination)) throw std::runtime_error("Asset pack destination must be empty");
    Json entries = Json::array();
    for (const auto& entry : records_) {
        const auto& record = entry.second;
        const auto separator = record.virtualPath.find(":/");
        const auto relative = record.virtualPath.substr(0, separator) + "/" + record.virtualPath.substr(separator + 2);
        write(destination / relative, readSource(record.id));
        write((destination / relative).string() + ".azmeta", bytes(record.path.string() + ".azmeta"));
        entries.push_back({{"id", record.id}, {"path", relative}, {"contentHash", record.contentHash}, {"dependencies", record.dependencies}});
    }
    write(destination / "manifest.azurepack", Json{{"schemaVersion", 1}, {"assets", entries}}.dump(2));
}
std::filesystem::path AssetDatabase::resolvePack(const std::filesystem::path& directory, const std::string& id) {
    const auto document = Json::parse(bytes(directory / "manifest.azurepack"));
    if (!document.at("schemaVersion").is_number_integer() || document.at("schemaVersion") != 1) throw std::runtime_error("Unsupported asset pack version");
    for (const auto& entry : document.at("assets")) if (entry.at("id") == id) {
        const std::filesystem::path relative(entry.at("path").get<std::string>());
        const auto base = std::filesystem::weakly_canonical(directory), result = std::filesystem::weakly_canonical(base / relative);
        const auto inside = result.lexically_relative(base);
        if (relative.is_absolute() || inside.empty() || *inside.begin() == ".." || hash(bytes(result)) != entry.at("contentHash").get<std::uint64_t>())
            throw std::runtime_error("Invalid asset pack entry");
        return result;
    }
    throw std::runtime_error("Asset absent from pack: " + id);
}
} // namespace azurerender
