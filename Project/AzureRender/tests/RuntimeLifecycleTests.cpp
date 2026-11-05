#include <iostream>
#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <limits>
#include <new>
#include <algorithm>
#include <stdexcept>

#include "runtime/RuntimeLifecycle.hpp"
#include "scene/SceneDescription.hpp"
namespace allocations {
std::atomic<std::size_t> live{0},peak{0},total{0};
struct Header {void* allocation;std::size_t size;};
void* acquire(std::size_t size,std::size_t alignment=alignof(std::max_align_t)) {
    size=std::max<std::size_t>(size,1);total.fetch_add(size,std::memory_order_relaxed);
    alignment=std::max(alignment,alignof(std::max_align_t));
    if(size>std::numeric_limits<std::size_t>::max()-alignment-sizeof(Header))throw std::bad_alloc();
    auto* raw=std::malloc(size+alignment+sizeof(Header));
    if(!raw)throw std::bad_alloc();
    auto address=(reinterpret_cast<std::uintptr_t>(raw)+sizeof(Header)+alignment-1)&~(alignment-1);
    auto* header=reinterpret_cast<Header*>(address)-1;*header={raw,size};
    const auto current=live.fetch_add(size,std::memory_order_relaxed)+size;
    auto previous=peak.load(std::memory_order_relaxed);
    while(previous<current&&!peak.compare_exchange_weak(previous,current,std::memory_order_relaxed)){}
    return reinterpret_cast<void*>(address);
}
void release(void* pointer) noexcept {
    if(!pointer)return;
    const auto* header=static_cast<Header*>(pointer)-1;
    live.fetch_sub(header->size,std::memory_order_relaxed);std::free(header->allocation);
}
}
void* operator new(std::size_t size){return allocations::acquire(size);}
void* operator new[](std::size_t size){return allocations::acquire(size);}
void operator delete(void* pointer) noexcept {allocations::release(pointer);}
void operator delete[](void* pointer) noexcept {allocations::release(pointer);}
void operator delete(void* pointer,std::size_t) noexcept {allocations::release(pointer);}
void operator delete[](void* pointer,std::size_t) noexcept {allocations::release(pointer);}
void* operator new(std::size_t size,std::align_val_t alignment){return allocations::acquire(size,static_cast<std::size_t>(alignment));}
void* operator new[](std::size_t size,std::align_val_t alignment){return allocations::acquire(size,static_cast<std::size_t>(alignment));}
void operator delete(void* pointer,std::align_val_t) noexcept {allocations::release(pointer);}
void operator delete[](void* pointer,std::align_val_t) noexcept {allocations::release(pointer);}
void operator delete(void* pointer,std::size_t,std::align_val_t) noexcept {allocations::release(pointer);}
void operator delete[](void* pointer,std::size_t,std::align_val_t) noexcept {allocations::release(pointer);}
void* operator new(std::size_t size,const std::nothrow_t&) noexcept {try{return allocations::acquire(size);}catch(...){return nullptr;}}
void* operator new[](std::size_t size,const std::nothrow_t&) noexcept {try{return allocations::acquire(size);}catch(...){return nullptr;}}
void operator delete(void* pointer,const std::nothrow_t&) noexcept {allocations::release(pointer);}
void operator delete[](void* pointer,const std::nothrow_t&) noexcept {allocations::release(pointer);}
void* operator new(std::size_t size,std::align_val_t alignment,const std::nothrow_t&) noexcept {try{return allocations::acquire(size,static_cast<std::size_t>(alignment));}catch(...){return nullptr;}}
void* operator new[](std::size_t size,std::align_val_t alignment,const std::nothrow_t&) noexcept {try{return allocations::acquire(size,static_cast<std::size_t>(alignment));}catch(...){return nullptr;}}
void operator delete(void* pointer,std::align_val_t,const std::nothrow_t&) noexcept {allocations::release(pointer);}
void operator delete[](void* pointer,std::align_val_t,const std::nothrow_t&) noexcept {allocations::release(pointer);}

using azurerender::RuntimeLifecycle;
void require(bool v, const char* message = "Lifecycle assertion failed") {
    if (!v) throw std::runtime_error(message);
}
void checkNodeIdentityLookup() {
    RuntimeLifecycle runtime;
    require(runtime.nodeId(azurerender::ecs::kInvalidEntity).empty(), "Invalid entity has no node identity");
    azurerender::SceneDocument document;
    for (const auto* id : {"first", "second"}) {
        azurerender::SceneNode node;
        node.id = id;
        document.nodes.push_back(node);
    }
    runtime.loadScene(document);
    const auto first = runtime.entity("first");
    const auto second = runtime.entity("second");
    require(runtime.nodeId(first) == "first" && runtime.nodeId(second) == "second",
        "Live node identities must resolve through the owning runtime");
    const auto anonymous = runtime.world().createEntity();
    require(runtime.nodeId(anonymous).empty(), "Anonymous ECS entities have no document identity");

    RuntimeLifecycle other;
    auto foreign = document;
    foreign.nodes.resize(1);
    foreign.nodes[0].id = "foreign";
    other.loadScene(foreign);
    runtime.world().swap(other.world());
    require(runtime.nodeId(first).empty() && other.nodeId(first).empty(),
        "An identity component from another document must reject despite matching numeric entity IDs");
    runtime.world().swap(other.world());
    require(runtime.nodeId(first) == "first", "Restored ownership must resolve the original node");

    runtime.world().destroyEntity(first);
    const auto recycled = runtime.world().createEntity();
    require(recycled == first && runtime.nodeId(recycled).empty(),
        "A recycled entity must not inherit the destroyed document node identity");
    require(runtime.entity("first") == azurerender::ecs::kInvalidEntity,
        "The forward lookup must also reject a recycled anonymous entity");
    auto replacement = foreign;
    replacement.nodes[0].id = "replacement";
    runtime.replaceScene(replacement);
    const auto current = runtime.entity("replacement");
    require(runtime.nodeId(current) == "replacement" && runtime.entity("second") == azurerender::ecs::kInvalidEntity,
        "Scene replacement must publish only the replacement document identities");
    bool rejected = false;
    try {
        runtime.replaceScene(document, [](RuntimeLifecycle&) { throw std::runtime_error("configuration rejected"); });
    } catch (const std::runtime_error&) { rejected = true; }
    require(rejected && runtime.nodeId(current) == "replacement",
        "Failed scene replacement must preserve the original identity ownership");
    runtime.start();
    azurerender::SceneNode spawned;
    spawned.id = "spawned";
    runtime.deferSpawn(spawned);
    require(runtime.entity("spawned") == azurerender::ecs::kInvalidEntity,
        "Deferred nodes acquire identities at their commit boundary");
    runtime.beginFrame(.1);
    require(runtime.nodeId(runtime.entity("spawned")) == "spawned", "Committed spawns must resolve their identity");
    runtime.stop();
    require(runtime.nodeId(current).empty(), "Stopped worlds release node identities");
}


void checkSnapshotCopyBudget() {
    using namespace azurerender;
    SceneDocument source;source.sceneId="snapshot-material-ownership";
    source.resources.push_back({"resource","model","assets:/models/mesh.gltf"});
    for(unsigned i=0;i<1024;++i) {
        SceneNode node;node.id="snapshot-node-with-stable-identity-"+std::to_string(i);
        node.name=std::string(96,'n')+std::to_string(i);node.resourceId="resource";
        if(i)node.parentId=source.nodes.front().id;
        node.prefabSource="assets:/prefabs/shared-renderable.azureprefab";node.instanceOf="prefab-instance-with-independent-snapshot";
        for(unsigned slot=0;slot<4;++slot) {
            const auto key="material-slot-"+std::to_string(slot);
            node.materialSlots[key]="assets:/materials/snapshot-material-"+std::to_string(slot)+".azurematerial";
            MaterialBinding binding;binding.path=node.materialSlots.at(key);binding.fingerprint=std::string(64,'f');
            binding.textures["baseColor"]="assets:/textures/snapshot-diffuse.png";
            binding.textures["normal"]="assets:/textures/snapshot-normal.png";
            node.resolvedMaterials[key]=std::move(binding);
        }
        node.objectShadow=ecs::ObjectShadowComponent{};
        source.nodes.push_back(std::move(node));
    }
    for(unsigned i:{0U,13U,1023U}) {SceneLight light;light.id="light-"+std::to_string(i);light.nodeId=source.nodes[i].id;source.lights.push_back(light);}
    RuntimeLifecycle runtime;runtime.loadScene(source);runtime.start();
    auto warmup=runtime.snapshotScene();
    const auto copyStart=allocations::total.load();auto ordinaryCopy=source;
    const auto ordinaryBytes=allocations::total.load()-copyStart;
    const auto snapshotStart=allocations::total.load();auto first=runtime.snapshotScene();
    const auto snapshotBytes=allocations::total.load()-snapshotStart;
    std::cout<<"Snapshot allocation bytes: "<<snapshotBytes<<", one document copy: "<<ordinaryBytes<<'\n';
    require(snapshotBytes<=ordinaryBytes+64*1024,"Snapshot allocations exceed one owned document copy plus64KiB working allowance");
    require(first.nodes.size()==source.nodes.size() && first.sceneId==source.sceneId && first.resources.size()==source.resources.size(),"Snapshot must preserve document metadata");
    for(std::size_t i=0;i<first.nodes.size();++i) {
        const auto& actual=first.nodes[i];const auto& expected=source.nodes[i];
        require(actual.id==expected.id && actual.name==expected.name && actual.parentId==expected.parentId && actual.prefabSource==expected.prefabSource && actual.instanceOf==expected.instanceOf && actual.materialSlots==expected.materialSlots,"Snapshot must preserve node order and authoring data");
        for(const auto& slot:expected.resolvedMaterials) {
            const auto& material=actual.resolvedMaterials.at(slot.first);
            require(material.path==slot.second.path && material.fingerprint==slot.second.fingerprint && material.textures==slot.second.textures,"Resolved material inputs must survive snapshotting");
        }
    }
    for(unsigned i:{0U,511U,1023U})runtime.world().destroyEntity(runtime.entity(source.nodes[i].id));
    const auto entity=runtime.entity(source.nodes[13].id);
    runtime.world().tryGet<ecs::TransformComponent>(entity)->translation={4,5,6};
    runtime.world().tryGet<ecs::RenderableComponent>(entity)->visible=false;
    runtime.world().tryGet<ecs::ObjectShadowComponent>(entity)->normalBias=.025F;
    const auto noShadow=runtime.entity(source.nodes[14].id);runtime.world().removeComponent<ecs::ObjectShadowComponent>(noShadow);
    auto compacted=runtime.snapshotScene();std::size_t cursor=0;
    for(unsigned i=0;i<source.nodes.size();++i) {
        if(i==0||i==511||i==1023)continue;
        const auto& actual=compacted.nodes.at(cursor++);
        require(actual.id==source.nodes[i].id && actual.parentId.empty(),"Removed nodes must preserve stable survivor order and clear invalid parents");
        require(actual.materialSlots==source.nodes[i].materialSlots,"Compaction must preserve material slots");
        if(i==13)require(actual.translation==std::array<float,3>{4,5,6} && !actual.visible && actual.objectShadow->normalBias==.025F,"World render state must reach compacted snapshots");
        if(i==14)require(!actual.objectShadow,"Removed object-shadow component must clear the snapshot field");
    }
    require(cursor==1021 && compacted.lights.size()==1 && compacted.lights.front().nodeId==source.nodes[13].id,"Removed-node lights must be filtered");
    require(first.nodes.size()==1024 && first.nodes[13].translation==source.nodes[13].translation && first.nodes[13].visible && first.nodes[13].objectShadow->normalBias!=.025F,"Earlier snapshots must retain independent values");
    compacted.nodes.front().materialSlots.clear();compacted.nodes.front().resolvedMaterials.clear();compacted.nodes.front().name="edited-copy";
    const auto again=runtime.snapshotScene();
    require(again.nodes.front().name==source.nodes[1].name && again.nodes.front().materialSlots.size()==4 && again.nodes.front().resolvedMaterials.size()==4,"Caller edits must preserve runtime-owned material data");
}

int main() {
    try {checkSnapshotCopyBudget();}
    catch(const std::exception& error){std::cerr<<error.what()<<std::endl;return 77;}
    checkNodeIdentityLookup();
    RuntimeLifecycle runtime;
    int updates = 0;
    azurerender::SceneDocument document;
    azurerender::SceneNode node;
    node.id = "root";
    document.nodes.push_back(node);
    document.lights.push_back({"light", "root"});
    runtime.loadScene(document);
    require(runtime.snapshotScene().nodes.size() == 1);
    auto nodes = runtime.world().componentArray<azurerender::ecs::TransformComponent>().entities();
    require(nodes.size() == 1);
    runtime.world().tryGet<azurerender::ecs::TransformComponent>(nodes[0])->translation[0] = 3;
    require(runtime.snapshotScene().nodes[0].translation[0] == 3);
    runtime.world().tryGet<azurerender::ecs::RenderableComponent>(nodes[0])->visible = false;
    const auto renderScene = runtime.snapshotScene().renderDescription();
    require(renderScene.nodes[0].translation[0] == 3);
    require(!renderScene.nodes[0].visible);
    require(renderScene.lights[0].position[0] == 3);
    runtime.world().addSystem([&](azurerender::ecs::World&) { ++updates; });
    runtime.world().destroyEntity(nodes[0]);
    auto replacement = runtime.world().createEntity();
    require(replacement == nodes[0]);
    require(runtime.snapshotScene().nodes.empty());
    const auto deletedScene = runtime.snapshotScene().renderDescription();
    require(deletedScene.nodes.empty() && deletedScene.lights.empty());
    runtime.start();
    require(runtime.beginFrame(0.1) == 0.1);
    require(updates == 1);
    runtime.pause();
    require(runtime.beginFrame(0.1) == 0);
    require(updates == 1);
    runtime.step();
    require(runtime.beginFrame(0.1) == 0.1);
    require(updates == 2);
    require(runtime.beginFrame(0.1) == 0);
    require(updates == 2);
    auto entity = runtime.world().createEntity();
    runtime.defer([entity](auto& w) { w.destroyEntity(entity); });
    runtime.beginFrame(0.1);
    require(!runtime.world().valid(entity));
    runtime.resume();
    runtime.beginFrame(0.1);
    require(updates == 3);
    bool invalid = false;
    try {
        runtime.beginFrame(-1);
    } catch (const std::invalid_argument&) {
        invalid = true;
    }
    require(invalid);
    runtime.stop();
    require(runtime.beginFrame(0.1) == 0);
    require(!runtime.world().valid(nodes[0]));
    bool closed = false;
    try {
        runtime.defer([](auto&) {});
    } catch (const std::logic_error&) {
        closed = true;
    }
    require(closed);
    std::cout << "Runtime lifecycle contracts passed\n";
}
