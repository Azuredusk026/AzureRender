#pragma once
#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

namespace azurerender {
// Single-owner thread. Callers retire GPU work before destroying the table.
// Snapshots retain providers' resource owners; native ownership stays with T.
template<class T> class ResourceIndexTable {
    struct Identity {};
public:
    struct Handle {
        std::uint32_t slot = 0;
        std::uint64_t generation = 0;
        std::shared_ptr<const Identity> owner;
    };
    struct Snapshot {
        std::shared_ptr<const T> resource;
        std::uint32_t slot;
        std::uint64_t generation;
    };
    explicit ResourceIndexTable(std::uint32_t capacity) : slots_(capacity) {
        if (!capacity) throw std::invalid_argument("Resource table capacity must be positive");
    }
    ResourceIndexTable(const ResourceIndexTable&) = delete;
    ResourceIndexTable& operator=(const ResourceIndexTable&) = delete;
    Handle insert(std::shared_ptr<const T> resource) {
        checkThread();
        if (!resource) throw std::invalid_argument("Null resource owner");
        for (std::size_t index=0; index<slots_.size(); ++index) {
            auto& slot=slots_[index];
            if (!slot.resource && slot.generation!=std::numeric_limits<std::uint64_t>::max()) {
                ++slot.generation; slot.resource=std::move(resource); slot.active=true; slot.serial=completed_;
                return {static_cast<std::uint32_t>(index),slot.generation,identity_};
            }
        }
        throw std::length_error("No reusable resource index slot");
    }
    Snapshot acquire(Handle handle,std::uint64_t submissionSerial) {
        auto& slot=validate(handle);
        if (submissionSerial<completed_ || submissionSerial<submitted_)
            throw std::invalid_argument("Submission serial must be monotonic and at least completion");
        submitted_=submissionSerial; slot.serial=std::max(slot.serial,submissionSerial);
        return {slot.resource,handle.slot,handle.generation};
    }
    Handle replace(Handle handle,std::shared_ptr<const T> resource) {
        auto& slot=validate(handle);
        if (!resource) throw std::invalid_argument("Null resource owner");
        if (slot.serial<=completed_ && slot.generation!=std::numeric_limits<std::uint64_t>::max()) {
            ++slot.generation; slot.resource=std::move(resource);
            return {handle.slot,slot.generation,identity_};
        }
        auto result=insert(std::move(resource)); // Failure leaves the original handle valid.
        slot.active=false;
        return result;
    }
    void erase(Handle handle) {
        auto& slot=validate(handle); slot.active=false;
        if (slot.serial<=completed_) slot.resource.reset();
    }
    void collect(std::uint64_t completedSerial) {
        checkThread();
        if (completedSerial<completed_) throw std::invalid_argument("Completion serial regressed");
        completed_=completedSerial;
        for(auto& slot:slots_) if(!slot.active && slot.serial<=completed_) slot.resource.reset();
    }
    std::size_t liveCount() const {
        checkThread(); return static_cast<std::size_t>(std::count_if(slots_.begin(),slots_.end(),
            [](const Slot& slot){return slot.active;}));
    }
    std::size_t retiredCount() const {
        checkThread(); return static_cast<std::size_t>(std::count_if(slots_.begin(),slots_.end(),
            [](const Slot& slot){return !slot.active && bool(slot.resource);}));
    }
private:
    struct Slot { std::shared_ptr<const T> resource; std::uint64_t generation=0,serial=0; bool active=false; };
    void checkThread() const {
        if(std::this_thread::get_id()!=thread_) throw std::logic_error("Resource table used outside owner thread");
    }
    Slot& validate(const Handle& handle) {
        checkThread();
        if(handle.owner!=identity_ || handle.slot>=slots_.size()) throw std::invalid_argument("Foreign resource handle");
        auto& slot=slots_[handle.slot];
        if(!slot.active || slot.generation!=handle.generation) throw std::invalid_argument("Stale resource handle");
        return slot;
    }
    std::vector<Slot> slots_;
    const std::shared_ptr<const Identity> identity_=std::make_shared<Identity>();
    const std::thread::id thread_=std::this_thread::get_id();
    std::uint64_t completed_=0,submitted_=0;
};
} // namespace azurerender
