#include "runtime/ComponentRegistry.hpp"
#include "runtime/GeneratedComponents.hpp"
#include "runtime/physics/PhysicsDescriptors.hpp"
#include "animation/ExpressionPose.hpp"
#include "render/ReferenceCameraCalibration.hpp"
namespace azurerender {
void ComponentRegistry::add(ComponentBinding value) {
    if(sealed_)throw std::logic_error("Component registration is sealed");
    metadata_->type(value.type);
    if (!value.install || !value.encode || !value.remove || !value.contains || !value.defaults || !value.validate)
        throw std::invalid_argument("Incomplete component binding: " + value.type);
    const auto name = value.type;
    if (!bindings_.emplace(name, std::move(value)).second) throw std::invalid_argument("Duplicate component binding: " + name);
}
const ComponentBinding& ComponentRegistry::binding(const std::string& type) const {
    auto found = bindings_.find(type);
    if (found == bindings_.end()) throw std::invalid_argument("Unknown component binding: " + type);
    return found->second;
}
void ComponentRegistry::install(const std::string& type, ecs::World& world, ecs::Entity entity, const Json& value) const {
    if (!world.valid(entity)) throw std::invalid_argument("Invalid component entity");
    binding(type).install(world, entity, value);
}
Json ComponentRegistry::encode(const std::string& type, const ecs::World& world, ecs::Entity entity) const {
    if (!world.valid(entity)) throw std::invalid_argument("Invalid component entity");
    return binding(type).encode(world, entity);
}
void ComponentRegistry::remove(const std::string& type, ecs::World& world, ecs::Entity entity) const {
    if (!world.valid(entity)) throw std::invalid_argument("Invalid component entity");
    binding(type).remove(world, entity);
}
bool ComponentRegistry::contains(const std::string& type, const ecs::World& world, ecs::Entity entity) const {
    const auto& value = binding(type);
    return world.valid(entity) && value.contains(world, entity);
}
Json ComponentRegistry::defaults(const std::string& type) const { return binding(type).defaults(); }
void ComponentRegistry::validate(const std::string& type, const Json& value) const { binding(type).validate(value); }
void ComponentRegistry::validateWrite(const std::string& type, const std::string& field, const Json& value, bool toolAccess) const {
    const auto& descriptor = metadata_->type(type);
    for (const auto& property : descriptor.properties) {
        if (property.name != field) continue;
        if (property.readOnly || (toolAccess && !property.toolVisible)) throw std::invalid_argument("Property is not writable: " + field);
        if (!property.validate) throw std::invalid_argument("Property has no validator: " + field);
        property.validate(value); return;
    }
    throw std::invalid_argument("Unknown property: " + field);
}
Json ComponentRegistry::describe(const std::string& type) const {
    binding(type);
    const auto& descriptor = metadata_->type(type);
    Json properties = Json::array();
    for (const auto& property : descriptor.properties)
        properties.push_back({{"name", property.name}, {"label", property.label}, {"kind", static_cast<int>(property.kind)},
            {"minimum", property.minimum}, {"maximum", property.maximum}, {"category", property.category},
            {"tooltip", property.tooltip}, {"readOnly", property.readOnly}, {"toolVisible", property.toolVisible},
            {"unit",property.unit},{"precision",property.precision},{"batchEditable",property.batchEditable},
            {"reference",property.reference},{"assetTypes",property.assetTypes},{"referenceDefault",property.referenceDefault}});
    const auto found=presentations_.find(type);
    const auto info=found==presentations_.end()?ComponentPresentation{type,"扩展"}:found->second;
    std::string enabled,visible;
    for(const auto& field:descriptor.properties)if(field.kind==reflection::Kind::Boolean){
        if(field.name=="enabled")enabled=field.name;
        if(field.name=="visible")visible=field.name;
    }
    return {{"type", type}, {"id", descriptor.id}, {"version", descriptor.version}, {"properties", properties},
        {"label",info.label},{"category",info.category},{"requires",info.requires},{"conflicts",info.conflicts},
        {"removable",info.removable},{"multiple",false},{"enabledField",enabled},{"visibleField",visible}};
}
void ComponentRegistry::validateComposition(const Json& components) const {
    for (const auto& component : components.items()) {
        const auto& type = component.key();
        binding(type);
        const auto presentation = presentations_.find(type);
        if (presentation == presentations_.end()) continue;
        for (const auto& required : presentation->second.requires) {
            if (!components.contains(required))
                throw std::invalid_argument(type + ": missing component " + required);
        }
        for (const auto& conflict : presentation->second.conflicts) {
            if (components.contains(conflict))
                throw std::invalid_argument(type + ": incompatible component " + conflict);
        }
    }
}
ComponentRegistry makeRuntimeComponentRegistry() {
    ComponentRegistry result(reflection::makeRuntimeRegistry()); registerGeneratedComponents(result);
    result.setPresentation("azure.transform",{"变换","场景",{},{},false});
    result.setPresentation("azure.renderable",{"渲染可见性","场景",{},{},false});
    result.setPresentation("azure.object-shadow",{"对象阴影","呈现"});
    result.setPresentation("azure.rigid-body",{"刚体","物理",{}, {"azure.character"}});
    result.setPresentation("azure.character",{"角色控制器","物理",{}, {"azure.rigid-body"}});
    result.setPresentation("azure.hinge",{"铰链约束","物理"});
    result.setPresentation("azure.camera",{"游戏相机","场景"});
    result.setPresentation("azure.reference-camera",{"参考相机","场景",{"azure.camera"}});
    result.addValidation("azure.reference-camera",[](const Json& value){
        const auto name=value.at("view").get<std::string>();if(name.size()>64||(value.at("enabled").get<bool>()&&name.empty()))throw std::invalid_argument("Enabled reference camera requires a view name of1..64 bytes");
        validateReferenceCameraPose({value.at("anchorPosition").get<std::array<float,3>>(),value.at("anchorRight").get<std::array<float,3>>(),value.at("anchorUp").get<std::array<float,3>>(),value.at("anchorBack").get<std::array<float,3>>()});
    });
    result.addValidation("azure.camera",[](const Json& value){if(value.at("nearPlane").get<float>()>=value.at("farPlane").get<float>())throw std::invalid_argument("Camera requires near < far");});
    result.setPresentation("azure.third-person-camera",{"第三人称相机","场景"});
    result.setPresentation("azure.animator",{"动画播放器","呈现"});
    result.addValidation("azure.animator",[](const Json& value){
        static_cast<void>(morphOverridesFromJson(value.value("morphOverrides",std::string())));
        const auto expression=value.value("expression",std::string());
        if(!expression.empty())static_cast<void>(expressionPoseFromJson(Json::parse(expression)));
    });
    result.setPresentation("azure.audio-source",{"音频源","呈现"});
    result.setPresentation("azure.game-ui",{"游戏界面文档","呈现"});
    result.setPresentation("azure.screen-noise",{"单色屏幕噪声","呈现"});
    result.setPresentation("azure.screen-blackout",{"屏幕黑场","呈现"});
    result.setPresentation("azure.screen-chromatic",{"屏幕色差","呈现"});
    result.setPresentation("azure.screen-move",{"屏幕偏移","呈现"});
    result.setPresentation("azure.window-presentation",{"窗口显示设置","呈现"});
    result.setPresentation("azure.script",{"脚本","行为"});
    for(const auto& entry:std::vector<std::pair<std::string,std::string>>{{"interactable","交互目标"},{"collectible","收集物"},{"door","门状态"},{"checkpoint","检查点"},{"task-state","任务状态"}})
        result.setPresentation("azure."+entry.first,{entry.second,"应用行为"});
    result.addValidation("azure.rigid-body",[](const Json& value){
        game::RigidBody body;body.shape=value.at("shape");body.radius=value.at("radius");body.halfHeight=value.at("halfHeight");body.halfExtent=value.at("halfExtent");body.dynamic=value.at("dynamic");body.kinematic=value.at("kinematic");body.trigger=value.at("trigger");body.mesh=value.at("mesh");validateRigidBody(body);
    });
    result.addValidation("azure.hinge",[](const Json& value){
        game::Hinge hinge;hinge.bodyA=value.at("bodyA");hinge.bodyB=value.at("bodyB");hinge.axis=value.at("axis");hinge.normal=value.at("normal");hinge.anchorA=value.at("anchorA");hinge.anchorB=value.at("anchorB");hinge.minimumAngle=value.at("minimumAngle");hinge.maximumAngle=value.at("maximumAngle");validateHinge(hinge,false);
    });return result;
}
ComponentRegistry& runtimeComponentRegistry() { static auto registry = makeRuntimeComponentRegistry(); return registry; }
}
