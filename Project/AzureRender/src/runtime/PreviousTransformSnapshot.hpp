#pragma once
#include "ecs/World.hpp"
#include "ecs/Components.hpp"
#include <map>
#include <string>
#include <unordered_map>
#include <vector>
namespace azurerender {
class RuntimeLifecycle;
// Owner-thread history. RuntimeLifecycle owns node identities; ordered entity
// handles and scene epochs delimit their lifetime. Values remain owned until
// the next capture. Large component layouts retain complete map traversal.
class PreviousTransformSnapshot final {
public:
 explicit PreviousTransformSnapshot(std::size_t limit=65536):limit_(std::min<std::size_t>(limit,65536)){}
 void capture(RuntimeLifecycle&);
 const ecs::TransformComponent* find(const std::string& node) const;
 void clear() noexcept;
 bool rebuilt() const noexcept{return rebuilt_;}
private:
 struct Entry{ecs::EntityHandle handle;ecs::TransformComponent value;};
 std::size_t limit_;RuntimeLifecycle* owner_=nullptr;std::uint64_t revision_=0;bool fallback_=false,rebuilt_=false;
 std::vector<Entry> entries_;std::unordered_map<std::string,std::size_t> indices_;
 std::map<std::string,ecs::TransformComponent> complete_;
};
}
