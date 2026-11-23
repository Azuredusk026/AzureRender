#pragma once
#include <nlohmann/json.hpp>
#include <string>

namespace azurerender {
// The application supplies this owner-thread service. Language backends consume
// the same selection/task contract without depending on native platform APIs.
class IMediaLibraryService {
public:
    virtual ~IMediaLibraryService() = default;
    virtual bool select(const std::string& kind, bool directory) = 0;
    virtual nlohmann::json entries(const std::string& kind) const = 0;
    virtual nlohmann::json status() = 0;
    virtual void cancel() = 0;
};
}
