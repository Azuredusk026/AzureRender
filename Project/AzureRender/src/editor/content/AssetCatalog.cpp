#include "AssetCatalog.hpp"
#include "runtime/AssetTypeRegistry.hpp"
#include <algorithm>
#include <limits>
#include <map>
#include <string_view>
#include <vector>

namespace azurerender {
nlohmann::json assetCatalog(const EditorContext& context, const std::string& filter) {
    std::map<std::string, nlohmann::json> rows;
    std::map<std::string, std::string> identities;
    if (context.isProject()) {
        for (const auto& [id, record] : context.assets().records()) {
            // The database stores the canonical path returned by Project::resolve.
            identities[record.path.generic_u8string()] = id;
            const auto* type = assetTypeRegistry().classify(record.path);
            rows[id] = {{"id", id}, {"path", record.virtualPath}, {"source", record.path.u8string()},
                        {"type", type ? type->id : "unknown"}, {"ready", std::filesystem::is_regular_file(record.path)},
                        {"users", 0}, {"resources", nlohmann::json::array()}};
        }
    }

    std::map<std::string, std::size_t> sceneRanks;
    for (const auto& resource : context.resourceStatuses()) {
        auto path = resource.path;
        if (context.isProject()) {
            const auto reference = path.generic_u8string();
            if (reference.find(":/") != std::string::npos) {
                try {
                    path = context.assets().resolveReference(reference);
                } catch (const std::exception&) {}
            }
        }
        std::error_code error;
        const auto canonical = std::filesystem::weakly_canonical(path, error);
        const auto found = identities.find((error ? path.lexically_normal() : canonical).generic_u8string());
        const auto& id = found == identities.end() ? resource.id : found->second;
        sceneRanks.emplace(id, sceneRanks.size());
        auto [position, inserted] = rows.try_emplace(id);
        auto& row = position->second;
        if (inserted) {
            const auto* type = assetTypeRegistry().classify(path);
            row = {{"id", id}, {"path", path.filename().u8string()}, {"source", path.u8string()},
                   {"type", type ? type->id : "unknown"}, {"ready", std::filesystem::is_regular_file(path)},
                   {"users", 0}, {"resources", nlohmann::json::array()}};
        }
        row["users"] = row["users"].get<std::size_t>() + resource.dependentNodeCount;
        row["resources"].push_back(resource.id);
    }

    struct RankedRow {
        std::size_t rank;
        std::string_view path;
        nlohmann::json* row;
    };
    const auto unranked = std::numeric_limits<std::size_t>::max();
    std::vector<RankedRow> ordered;
    ordered.reserve(rows.size());
    for (auto& [id, row] : rows) {
        if (!filter.empty() && row.at("type") != filter) continue;
        const auto rank = sceneRanks.find(id);
        ordered.push_back({rank == sceneRanks.end() ? unranked : rank->second,
                           row.at("path").get_ref<const std::string&>(), &row});
    }
    std::sort(ordered.begin(), ordered.end(), [](const RankedRow& left, const RankedRow& right) {
        if (left.rank != right.rank) return left.rank < right.rank;
        return left.path < right.path;
    });
    auto result = nlohmann::json::array();
    result.get_ref<nlohmann::json::array_t&>().reserve(ordered.size());
    for (const auto& entry : ordered) result.push_back(std::move(*entry.row));
    return result;
}
}
