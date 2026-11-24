#include "editor/commands/EditRegistry.hpp"
#include "editor/EditorSession.hpp"
#include "runtime/ComponentRegistry.hpp"
#include "runtime/LevelRenderSettings.hpp"
#include <algorithm>
#include <fstream>
namespace azurerender {
EditRegistry editorOperations(EditorSession& session) {
    using Json=nlohmann::json;
    EditRegistry registry;
    const Json text={{"type","string"}},boolean={{"type","boolean"}},integer={{"type","integer"}},number={{"type","number"}};
    const Json vector={{"type","array"},{"minItems",3},{"maxItems",3},{"items",number}};
    auto schema=[](Json fields=Json::object(),Json required=Json::array()) {
        return Json{{"type","object"},{"properties",fields},{"required",required}};
    };
    auto document=[&](const std::string& id,Json parameters,auto handler) {
        registry.add({id,1,std::move(parameters),true,true,true},[handler](EditorContext& context,const Json& args)->Json {
            handler(context,args);return nullptr;
        });
    };
    document("node.create",schema({{"id",text}},{"id"}),[](auto& c,const auto& a) { c.createNode(a.at("id").template get<std::string>()); });
    document("node.rename",schema({{"value",text}},{"value"}),[](auto& c,const auto& a) {
        if(!c.selectedNode())throw std::invalid_argument("Rename requires a selected node");
        c.setSelectedNodeName(a.at("value").template get<std::string>());
    });
    document("node.visible",schema({{"value",boolean}},{"value"}),[](auto& c,const auto& a) { c.setSelectedNodeVisible(a.at("value").template get<bool>()); });
    document("node.prefab-source",schema({{"value",text}},{"value"}),[](auto& c,const auto& a) { c.setSelectedNodePrefab(a.at("value").template get<std::string>()); });
    document("node.instance",schema({{"value",text}},{"value"}),[](auto& c,const auto& a) { c.setSelectedNodeInstance(a.at("value").template get<std::string>()); });
    document("node.child",schema({{"parent",integer}},{"parent"}),[](auto& c,const auto& a) { c.addChildNode(a.at("parent").template get<std::size_t>()); });
    document("node.remove",schema({{"index",integer}},{"index"}),[](auto& c,const auto& a) { c.removeNode(a.at("index").template get<std::size_t>()); });
    document("node.duplicate",schema(),[](auto& c,const auto&) { c.duplicateSelection(); });
    document("node.delete",schema(),[](auto& c,const auto&) { c.deleteSelection(); });
    document("node.place",schema({{"resource",text},{"id",text}},{"resource"}),[](auto& c,const auto& a) {
        c.placeResource(a.at("resource").template get<std::string>(),a.value("id",std::string()));
    });
    document("prefab.place",schema({{"asset",text},{"instance",text}},{"asset","instance"}),[](auto& c,const auto& a) {
        c.placePrefab(a.at("asset").template get<std::string>(),a.at("instance").template get<std::string>());
    });
    document("node.transform",schema({{"translation",vector},{"rotation",vector},{"scale",vector}}),[](auto& c,const auto& a) {
        if(a.empty()||!c.selectedNode())throw std::invalid_argument("Transform requires fields and selection");
        for(const auto& field:a.items())runtimeComponentRegistry().validateWrite("azure.transform",field.key(),field.value());
        if(a.contains("scale"))for(const auto& value:a.at("scale"))
            if(value.template get<float>()==0)throw std::invalid_argument("Transform scale must be nonzero");
        if(a.contains("translation"))c.setGizmoTranslation(a.at("translation").template get<std::array<float,3>>());
        if(a.contains("rotation"))c.setGizmoRotation(a.at("rotation").template get<std::array<float,3>>());
        if(a.contains("scale"))c.setGizmoScale(a.at("scale").template get<std::array<float,3>>());
    });
    document("component.add",schema({{"type",text}},{"type"}),[](auto& c,const auto& a) { c.addGameplayComponent(a.at("type").template get<std::string>()); });
    document("component.field",schema({{"type",text},{"field",text},{"value",Json::object()}},{"type","field","value"}),[](auto& c,const auto& a) {
        c.setComponentField(a.at("type").template get<std::string>(),a.at("field").template get<std::string>(),a.at("value"));
    });
    document("render.settings",schema({{"values",Json::object()}},{"values"}),[](auto& c,const auto& a) {
        auto settings=c.renderSettings();const auto& fields=a.at("values");const auto known=encodeLevelRenderSettings(settings);
        if(!fields.is_object())throw std::invalid_argument("Render settings require an object");
        for(const auto& field:fields.items())if(!known.contains(field.key()))throw std::invalid_argument("Unknown render setting: "+field.key());
        decodeLevelRenderSettings(settings,fields);c.beginEdit();c.renderSettings()=settings;
    });
    document("render.preset",schema({{"value",integer}},{"value"}),[](auto& c,const auto& a) {
        const auto value=a.at("value").template get<unsigned>();if(value>4)throw std::invalid_argument("Unknown showcase preset");
        c.beginEdit();applyShowcasePresetLook(c.renderSettings(),value);
    });
    registry.add({"node.select",1,schema({{"id",text},{"index",integer},{"indices",{{"type","array"},{"items",integer}}}}),false,true,false},
        [](EditorContext& c,const Json& a)->Json {
            if(a.size()!=1)throw EditRejection("Selection requires one identity or index collection");
            if(a.contains("id")) {
                const auto id=a.at("id").get<std::string>();const auto& nodes=c.scene().nodes;
                const auto found=std::find_if(nodes.begin(),nodes.end(),[&](const auto& node) { return node.id==id; });
                if(found==nodes.end())throw std::invalid_argument("Unknown selected node: "+id);
                c.selectNode(static_cast<std::size_t>(found-nodes.begin()));
            }else if(a.contains("indices"))c.selectNodes(a.at("indices").get<std::vector<std::size_t>>());
            else c.selectNode(a.at("index").get<std::size_t>());
            return nullptr;
        });
    const auto settingSource=[](const Json& a) {
        const auto source=a.value("source",std::string("console"));
        if(source=="console")return SettingSource::Console;
        if(source=="user-file")return SettingSource::UserFile;
        throw EditRejection("Editor writes require console or user-file source");
    };
    registry.add({"settings.set",1,schema({{"name",text},{"value",Json::object()},{"source",text}},{"name","value"}),false,false,false},
        [&session,settingSource](EditorContext&,const Json& a)->Json {
            const auto result=session.settings().set(a.at("name").get<std::string>(),a.at("value"),settingSource(a));
            if(!result.passed)throw EditRejection(result.diagnostic);return {{"queued",true}};
        });
    registry.add({"settings.reset",1,schema({{"name",text},{"source",text}},{"name"}),false,false,false},
        [&session,settingSource](EditorContext&,const Json& a)->Json {
            const auto result=session.settings().reset(a.at("name").get<std::string>(),settingSource(a));
            if(!result.passed)throw EditRejection(result.diagnostic);return {{"queued",true}};
        });
    registry.add({"settings.describe",1,schema({{"search",text}}),false,false,false},
        [&session](EditorContext&,const Json& a)->Json{return session.settings().describe(a.value("search",std::string()));});
    for(const auto& item:std::vector<std::pair<std::string,EditorCommand>>{
            {"document.save",EditorCommand::Save},{"document.reload",EditorCommand::Reload},
            {"history.undo",EditorCommand::Undo},{"history.redo",EditorCommand::Redo},
            {"preview.play",EditorCommand::Play},{"preview.pause",EditorCommand::Pause},{"preview.resume",EditorCommand::Resume},
            {"preview.step",EditorCommand::Step},{"preview.stop",EditorCommand::Stop},{"workspace.reset",EditorCommand::ResetLayout},
            {"assets.reload",EditorCommand::ReloadAssets},{"viewport.capture",EditorCommand::Capture}}) {
        const bool idle=item.second==EditorCommand::Save||item.second==EditorCommand::Reload||item.second==EditorCommand::Undo||item.second==EditorCommand::Redo;
        registry.add({item.first,1,schema(),idle,false,idle},[&session,command=item.second](EditorContext&,const Json&)->Json {
            if(!session.executeInternal(command))throw EditRejection(session.lastError().empty()?"Operation unavailable":session.lastError());
            return nullptr;
        });
    }
    registry.add({"project.build",1,schema({{"install",text},{"output",text},{"replace",boolean}},{"install","output"}),true,false,true},
        [&session](EditorContext&,const Json& a)->Json {
            if(!session.startBuildInternal(a.at("install").get<std::string>(),a.at("output").get<std::string>(),a.value("replace",false)))
                throw EditRejection(session.lastError());
            return nullptr;
        });
    registry.add({"project.create",1,schema({{"path",text},{"name",text}},{"path","name"}),false,false,true},
        [](EditorContext& c,const Json& a)->Json {
            Project::createGame(a.at("path").get<std::string>(),a.at("name").get<std::string>());
            c.log("Game template created: "+a.at("path").get<std::string>());return nullptr;
        });
    registry.add({"asset.import",1,schema({{"path",text}},{"path"}),true,false,true},[](EditorContext& c,const Json& a)->Json { return c.importAsset(a.at("path").get<std::string>()); });
    registry.add({"asset.generate",1,schema({{"generator",text},{"output",text},{"parameters",Json::object()},
        {"inputs",{{"type","array"},{"maxItems",1024},{"items",text}}},{"dependencies",{{"type","array"},{"maxItems",1024},{"items",text}}},{"license",text}},
        {"generator","output","parameters","license"}),true,false,true},[&session](EditorContext& c,const Json& a)->Json {
            if(!c.isProject())throw EditRejection("Asset generation requires a project session");
            const auto output=a.at("output").get<std::string>();const auto extension=std::filesystem::path(output).extension();
            if(extension!=".azurelevel"&&extension!=".azureprefab"&&extension!=".json"&&extension!=".gltf"&&extension!=".glb")throw EditRejection("Generated content requires a registered engine format");
            const auto validator=[extension](const std::string& source) {
                if(extension!=".glb") { const auto data=Json::parse(source);if(!data.is_object())throw EditRejection("Generated text requires an object"); }
            };
            const auto id=c.assets().generateAsset(session.generators(),a.at("generator").get<std::string>(),output,a.at("parameters"),
                a.value("inputs",std::vector<std::string>{}),a.value("dependencies",std::vector<std::string>{}),a.at("license").get<std::string>(),validator);
            c.notifyAssetVersionChanged();session.assetReloadRequested_=true;c.log("Generated asset: "+output);return id;
        });
    registry.add({"asset.import-start",1,schema({{"path",text}},{"path"}),false,false,true},[](EditorContext& c,const Json& a)->Json { c.startImport(a.at("path").get<std::string>());return nullptr; });
    registry.add({"asset.import-cancel",1,schema(),false,false,false},[](EditorContext& c,const Json&)->Json { c.cancelImport();return nullptr; });
    registry.add({"asset.import-poll",1,schema(),true,false,true},[](EditorContext& c,const Json&)->Json { const auto value=c.pollImport();return value?Json(*value):Json(nullptr); });
    registry.add({"animation.preview",1,schema({{"state",text},{"time",number},{"previous",text},{"crossfade",number}},{"state"}),false,false,true},
        [](EditorContext& c,const Json& a)->Json { c.previewAnimation(a.at("state").get<std::string>(),a.value("time",0.0),a.value("previous",std::string()),a.value("crossfade",0.0));return nullptr; });
    registry.add({"animation.clear-preview",1,schema(),false,false,true},[](EditorContext& c,const Json&)->Json { c.clearAnimationPreview();return nullptr; });
    registry.add({"viewport.gizmo-mode",1,schema({{"value",integer}},{"value"}),false,false,true},
        [](EditorContext& c,const Json& a)->Json {
            const auto mode=a.at("value").get<unsigned>();if(mode>2)throw EditRejection("Unknown gizmo mode");
            c.setGizmoMode(static_cast<EditorContext::GizmoMode>(mode));return nullptr;
        });
    registry.add({"viewport.debug-overlay",1,schema({{"enabled",boolean}},{"enabled"}),false,false,false},
        [&session](EditorContext&,const Json& a)->Json { session.debugOverlay=a.at("enabled").get<bool>();return nullptr; });
    registry.add({"preview.level",1,schema({{"value",text}},{"value"}),false,false,false},
        [&session](EditorContext&,const Json& a)->Json {
            if(!session.levels())throw EditRejection("Level requires Play");
            session.levels()->request(a.at("value").get<std::string>());return nullptr;
        });
    registry.add({"script.write",1,schema({{"path",text},{"value",{{"type","string"},{"maxLength",2097152}}}},{"path","value"}),false,false,false},
        [&session](EditorContext& c,const Json& a)->Json {
            if(session.building())throw EditRejection("Script writes require a finished build");
            const auto reference=a.at("path").get<std::string>();
            if(reference.rfind("engine:/",0)==0)throw EditRejection("Script writes require a project mount");
            const auto path=c.project().resolve(reference);
            if(path.extension()!=".lua")throw EditRejection("Script write expects a Lua asset");
            const auto source=a.at("value").get<std::string>();
            if(source.size()>2097152)throw EditRejection("Script exceeds source budget");
            std::ofstream output(path);output<<source;output.close();
            if(!output)throw std::runtime_error("Cannot save script");
            if(session.scripts())session.scripts()->reloadChanged();return nullptr;
        });
    registerProposalOperations(registry,session);
    registerDeveloperOperations(registry,session);
    return registry;
}
}
