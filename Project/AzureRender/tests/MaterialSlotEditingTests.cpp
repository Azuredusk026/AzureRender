#include "editor/EditorSession.hpp"
#include "editor/preview/ThumbnailProviderRegistry.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>
using namespace azurerender;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
Json run(EditorSession& session,const std::string& operation,Json parameters=Json::object()){
    const auto result=session.edit(operation,parameters);
    if(!result)throw std::runtime_error(operation+": "+result.diagnostics.dump());return result.value;
}
}
int main(){
    const auto root=std::filesystem::temp_directory_path()/("azure-material-edit-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int status=0;
    try{
        Project::create(root,"Material authoring");auto context=EditorContext::openProject(root/"project.azureproject");EditorSession session(context);
        const auto content=context->documentContent();
        const auto first=run(session,"material.create",{{"type","azure.pbr"},{"destination","assets:/surface.azurematerial"}}).at("asset").get<std::string>();
        const auto second=run(session,"material.create",{{"type","azure.stylized"},{"destination","assets:/style.azurematerial"}}).at("asset").get<std::string>();
        const auto full=run(session,"material.create",{{"type","azure.profile"},{"destination","assets:/complete.azurematerial"}}).at("asset").get<std::string>();
        const auto fullOriginal=run(session,"material.describe",{{"asset",full}}).at("document");
        auto fullSurface=fullOriginal.at("surface");fullSurface["materials"][0]["alphaMode"]="MASK";
        fullSurface["materials"][0]["extras"]["azureRenderMaterial"]["class"]="fabric";
        run(session,"material.field",{{"asset",full},{"field","surface"},{"value",fullSurface}});
        run(session,"material.undo",{{"asset",full}});
        require(run(session,"material.describe",{{"asset",full}}).at("document")==fullOriginal,"Full profile undo must restore the complete document");
        run(session,"material.redo",{{"asset",full}});run(session,"material.save",{{"asset",full}});
        require(run(session,"material.describe",{{"asset",full}}).at("document").at("surface")==fullSurface,"Full profile edits must persist all fields");
        const auto patch=run(session,"material.create",{{"type","azure.pbr"},{"mode","parameters"},{"destination","assets:/patch.azurematerial"}}).at("asset").get<std::string>();
        require(run(session,"material.describe",{{"asset",patch}}).at("document").at("parameters").empty(),"Parameter overrides start with inherited source fields");
        run(session,"material.field",{{"asset",patch},{"field","emissiveStrength"},{"value",2.0}});
        run(session,"material.undo",{{"asset",patch}});
        require(run(session,"material.describe",{{"asset",patch}}).at("document").at("parameters").empty(),"Undo must restore inheritance rather than a numeric default");
        run(session,"material.redo",{{"asset",patch}});
        require(run(session,"material.describe",{{"asset",patch}}).at("document").at("schemaVersion")==2,"Sparse override mode must survive editing history");
        run(session,"material.field",{{"asset",patch},{"field","emissiveStrength"},{"value",nullptr}});
        require(!session.edit("material.field",{{"asset",patch},{"field","textures.typo"},{"value",nullptr}}),"Resetting an unknown texture must fail");
        require(run(session,"material.describe",{{"asset",patch}}).at("document").at("parameters").empty(),"Resetting an override restores inheritance");
        require(context->documentContent()==content&&!context->dirty(),"Material asset creation must preserve the scene history");
        require(run(session,"preview.types").size()>=3,"Model, image and material preview providers must be discoverable");
        require(run(session,"asset.open",{{"asset",first}}).at("operation")=="material.describe","Material double click must target its registered editor");
        require(!session.edit("material.create",{{"type","azure.pbr"},{"destination","assets:/surface.azurematerial"}}),"Creating a material must refuse overwrite");
        require(!session.edit("material.create",{{"type","unknown"},{"destination","assets:/invalid.azurematerial"}}),"Unregistered material types must fail before writing");
        run(session,"material.field",{{"asset",first},{"field","roughness"},{"value",.25}});
        require(run(session,"material.describe",{{"asset",first}}).at("document").at("parameters").at("roughness")==.25,"Parameter edits must update the actual material document");
        require(!session.edit("material.field",{{"asset",first},{"field","roughness"},{"value",2}}),"Out of range edits must be rejected");
        run(session,"material.undo",{{"asset",first}});
        require(run(session,"material.describe",{{"asset",first}}).at("document").at("parameters").at("roughness")==.75,"Material undo must restore its own history");
        run(session,"material.redo",{{"asset",first}});run(session,"material.save",{{"asset",first}});
        {std::ofstream image(root/"assets/color.ppm",std::ios::binary);image<<"P6\n1 1\n255\n";const unsigned char pixel[]={180,90,40};image.write(reinterpret_cast<const char*>(pixel),3);}
        context->assets().refresh();const auto imageId=context->assets().idForPath("assets:/color.ppm");
        run(session,"material.field",{{"asset",full},{"field","images"},{"value",{{"0",{{"asset",imageId},{"usage","linear"}}}}}});
        fullSurface["textures"]=Json::array({{{"source",0}}});
        fullSurface["materials"][0]["pbrMetallicRoughness"]["baseColorTexture"]={{"index",0}};
        run(session,"material.field",{{"asset",full},{"field","surface"},{"value",fullSurface}});
        require(context->assets().records().at(full).dependencies==std::vector<std::string>{imageId},"Full profile UUIDs must participate in asset dependency fingerprints");
        {const auto sidecar=root/"assets/color.ppm.azmeta";std::ifstream input(sidecar);Json metadata;input>>metadata;input.close();metadata["settings"]["texture"]={{"filter","nearest"},{"mipPolicy","none"},{"wrapU","clamp"}};std::ofstream output(sidecar);output<<metadata.dump();}
        context->assets().refresh();
        require(!session.edit("material.field",{{"asset",first},{"field","textures.albedo"},{"value",{{"asset",second},{"usage","albedo"}}}}),"Non-image texture references must be rejected without touching the asset");
        run(session,"material.field",{{"asset",first},{"field","textures.albedo"},{"value",{{"asset",imageId},{"usage","albedo"}}}});
        require(context->assets().records().at(first).dependencies==std::vector<std::string>{imageId},"Texture UUIDs must participate in dependency fingerprints");
        require(!session.edit("material.field",{{"asset",first},{"field","textures.normal"},{"value",{{"asset",imageId},{"usage","albedo"}}}}),"Texture slot usage must follow the registered type contract");
        RenderViewDescriptor settings;
        const auto sphere=ThumbnailProviderRegistry::builtins().describe(context->assets().records().at(first),context->assets(),root/".azure/previews",settings);
        const auto picture=ThumbnailProviderRegistry::builtins().describe(context->assets().records().at(imageId),context->assets(),root/".azure/previews",settings);
        require(sphere.view.scene.resources.size()==1&&!sphere.view.scene.resources[0].materials.empty(),"Material previews must bind an actual material to generated geometry");
        require(picture.view.scene.resources[0].materials.begin()->second.textures.count(imageId),"Image preview providers must retain the source image UUID");
        auto previewMesh=loadGltfAsset(sphere.view.scene.resources[0].path);applyMaterialBindings(previewMesh,sphere.view.scene.resources[0].materials);
        require(previewMesh.materials[0].baseColorWidth==1&&previewMesh.materials[0].baseColorPixels[0]>previewMesh.materials[0].baseColorPixels[1],"Preview runtime interpretation must load the real referenced pixels");
        require(previewMesh.materials[0].textureSettings[0].filter=="nearest"&&previewMesh.materials[0].textureSettings[0].mipPolicy=="none"&&previewMesh.materials[0].textureSettings[0].wrapU=="clamp","Independent materials must consume existing texture sampling contracts");
        const auto oldFingerprint=sphere.key.fingerprint;
        {std::ofstream image(root/"assets/color.ppm",std::ios::binary|std::ios::trunc);image<<"P6\n1 1\n255\n";const unsigned char pixel[]={40,90,180};image.write(reinterpret_cast<const char*>(pixel),3);}
        context->assets().refresh(true);
        const auto updated=ThumbnailProviderRegistry::builtins().describe(context->assets().records().at(first),context->assets(),root/".azure/previews",settings);
        require(updated.key.fingerprint!=oldFingerprint,"Changing a material texture must invalidate its preview dependency key");
        const auto model=run(session,"asset.generate",{{"generator","azure.primitive"},{"output","assets:/cube.glb"},{"parameters",{{"kind","cube"},{"size",{2,2,2}},{"segments",2}}},{"license","Azure primitive generator v1"}}).get<std::string>();
        run(session,"node.place",{{"resource",model},{"id","left"}});run(session,"node.place",{{"resource",model},{"id","right"}});
        const auto slots=run(session,"material.slots",{{"node","left"}}).at("slots");require(!slots.empty(),"Actual model must expose stable source slots");
        const auto slot=slots[0].at("id").get<std::string>();
        const auto undo=context->undoCount();
        run(session,"material.assign",{{"nodes",{"left","right"}},{"slotId",slot},{"material",first}});
        require(context->undoCount()==undo+1,"Multi-object assignment must be one scene transaction");
        require(run(session,"material.slots",{{"node","left"}}).at("slots")[0].at("override")==first,"Slot assignment must retain material UUID");
        require(!session.edit("material.assign",{{"nodes",{"left","missing"}},{"slotId",slot},{"material",second}}),"Invalid batch targets must leave all slots intact");
        require(!session.edit("material.assign",{{"nodes",{"left"}},{"slotId","invalid"},{"material",second}}),"Unknown slots must reject assignment");
        run(session,"history.undo");require(run(session,"material.slots",{{"node","left"}}).at("slots")[0].at("override").is_null(),"Scene undo must restore source slots");
        run(session,"history.redo");run(session,"material.assign",{{"nodes",{"right"}},{"slotId",slot},{"material",second}});
        run(session,"document.save");
        const auto reimport=run(session,"asset.reimport",{{"asset",model},{"settings",Json::object()},{"baseFingerprint",std::to_string(context->assets().records().at(model).fingerprint)}});
        for(unsigned i=0;i<2000;++i){session.pollTasks();if(session.tasks().report().back().at("state")!="running")break;std::this_thread::sleep_for(std::chrono::milliseconds(1));}
        const auto task=session.tasks().report().back();require(task.at("state")=="completed","Material-bound models must reimport through the common task service");
        require(task.at("result").contains("slotMapping")&&!task.at("result").at("slotMapping").empty(),"Model reimport must report stable slot mappings");
        require(task.at("result").at("slotDiagnostics").empty(),"Preserved stable slots must have no reimport mismatch diagnostics");
        auto& orphan=context->scene().nodes.front();const auto prior=orphan.materialSlots;const auto priorResource=orphan.resourceId;
        orphan.resourceId="resource-"+model;
        const auto actualResource=std::find_if(context->scene().resources.begin(),context->scene().resources.end(),[&](const auto& item){return item.path==context->assets().resolve(model);});
        require(actualResource!=context->scene().resources.end(),"Placed model must have a scene resource");orphan.resourceId=actualResource->id;orphan.materialSlots["missing-source-slot"]=first;
        const auto mapped=MaterialEditingService::reimportSlots(*context,model);require(!mapped.at("slotDiagnostics").empty(),"Orphan assignments must report the affected node and source slot");
        const auto taskCount=session.tasks().report().size();
        require(!session.edit("asset.reimport",{{"asset",model},{"settings",Json::object()},{"baseFingerprint",std::to_string(context->assets().records().at(model).fingerprint)}}),"Missing assigned slots must reject reimport before publication");
        require(session.tasks().report().size()==taskCount,"Invalid slot mappings must reject before creating a task");context->scene().nodes.front().materialSlots=prior;context->scene().nodes.front().resourceId=priorResource;
        auto reopened=EditorContext::openProject(root/"project.azureproject");EditorSession other(reopened);
        require(run(other,"material.describe",{{"asset",full}}).at("document").at("surface")==fullSurface,"Complete profiles must survive project reopening");
        require(run(other,"material.slots",{{"node","left"}}).at("slots")[0].at("override")==first,"Save and reopen must preserve UUID overrides");
        require(run(other,"material.describe",{{"asset",first}}).at("document").at("parameters").at("roughness")==.25,"Saved material parameters must survive reopening");
        run(other,"material.reset-slot",{{"nodes",{"left"}},{"slotId",slot}});
        require(run(other,"material.slots",{{"node","left"}}).at("slots")[0].at("override").is_null(),"Reset must restore the source material");
        std::cout<<"Two registered types, independent asset history, atomic slot edits and project reopening passed\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';status=1;}
    std::filesystem::remove_all(root);return status;
}
