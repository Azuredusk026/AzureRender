#include "SceneDocument.hpp"
#include "runtime/LevelRenderSettings.hpp"
#include "runtime/ColorGradingLutBinding.hpp"
#include "runtime/ObjectShadowComponentCodec.hpp"

#include <chrono>
#include <regex>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <random>
#include <stdexcept>
#include <system_error>

namespace azurerender {

namespace {

void requireHeader(std::istream& input, const std::string& expected) {
    std::string header;
    std::getline(input, header);
    if (header != expected) {
        throw std::runtime_error(
            "Invalid .azscene header; expected " + expected);
    }
}

bool readBool(std::istream& input, const char* name) {
    std::string key;
    bool value = false;
    if (!(input >> key >> std::boolalpha >> value) || key != name) {
        throw std::runtime_error(std::string("Missing .azscene field: ") + name);
    }
    return value;
}

// Writes the document payload through the supplied stream. Shared by the
// normal save path and the atomic-save path so both produce identical bytes.
void writeDocument(std::ostream& output, const SceneDocument& document) {
    output << "AzureRender Scene v1\n"
           << "schemaVersion " << SceneDocument::kSchemaVersion << '\n'
           << "renderSettingsVersion " << RenderSettings::kSchemaVersion << '\n'
           << "sceneId " << std::quoted(document.sceneId) << '\n'
           << "resourceCount " << document.resources.size() << '\n';
    for (const SceneResource& resource : document.resources) {
        output << "resource " << std::quoted(resource.id) << ' '
               << std::quoted(resource.type) << ' '
               << std::quoted(resource.path.generic_u8string()) << '\n';
    }
    output << "nodeCount " << document.nodes.size() << '\n';
    for (const SceneNode& node : document.nodes) {
        output << "node " << std::quoted(node.id) << ' '
               << std::quoted(node.name) << ' '
               << std::quoted(node.parentId) << ' '
               << std::quoted(node.resourceId) << '\n'
               << "visible " << std::boolalpha << node.visible << '\n'
               << "transform "
               << node.translation[0] << ' ' << node.translation[1] << ' '
               << node.translation[2] << ' ' << node.rotation[0] << ' '
               << node.rotation[1] << ' ' << node.rotation[2] << ' '
               << node.scale[0] << ' ' << node.scale[1] << ' '
               << node.scale[2] << '\n'
               << "prefab " << std::quoted(node.prefabSource) << ' '
               << std::quoted(node.instanceOf) << '\n';
        output<<"materialSlotCount "<<node.materialSlots.size()<<'\n';
        for(const auto& [slot,asset]:node.materialSlots)output<<"materialSlot "<<std::quoted(slot)<<' '<<std::quoted(asset)<<'\n';
        output<<"objectShadow "<<std::quoted(encodeObjectShadowComponent(node.objectShadow).dump())<<'\n';
    }
    output << "lightCount " << document.lights.size() << '\n';
    for (const SceneLight& light : document.lights) {
        light.shape.validate();
        output << "light " << std::quoted(light.id) << ' '
               << std::quoted(light.nodeId) << ' '
               << light.color[0] << ' ' << light.color[1] << ' '
               << light.color[2] << ' ' << light.intensity << ' '
               << light.radius << ' ' << std::boolalpha << light.enabled
               << '\n';
        output << "lightShape " << static_cast<int>(light.shape.type) << ' '
               << light.shape.direction[0] << ' ' << light.shape.direction[1] << ' '
               << light.shape.direction[2] << ' ' << light.shape.innerCone << ' '
               << light.shape.outerCone << '\n';
        output << "lightFalloff " << static_cast<int>(light.shape.falloff) << ' '
               << light.shape.falloffExponent << '\n';
        output << "lightInfluenceBox " << std::quoted(nlohmann::json(light.shape.influenceBox).dump()) << '\n';
        output << "lightSpecularScale " << light.shape.specularScale << '\n';
        output << "lightReceivers " << std::quoted(nlohmann::json(light.shape.receivers).dump()) << '\n';
        output << "lightCharacterResponse " << std::quoted((light.shape.characterResponse.present
            ?nlohmann::json(light.shape.characterResponse):nlohmann::json(nullptr)).dump()) << '\n';
    }
    output << "reflectionProbe " << std::quoted(nlohmann::json(document.reflectionProbe).dump()) << '\n';
    output << "irradianceVolume " << std::quoted(nlohmann::json(document.irradianceVolume).dump()) << '\n';
    output << "viewFog " << std::quoted(nlohmann::json(document.viewFog).dump()) << '\n';
    output << "showcasePreset " << document.renderSettings.showcasePreset << '\n'
           << "styleMaskStrength " << document.renderSettings.styleMaskStrength << '\n'
           << "diffuseBandThreshold " << document.renderSettings.diffuseBandThreshold << '\n'
           << "shadowMaximumFilterRadiusTexels "
           << document.renderSettings.shadow.maximumFilterRadiusTexels << '\n'
           << "cameraNear " << document.renderSettings.cameraNear << '\n'
           << "cameraFar " << document.renderSettings.cameraFar << '\n'
           << "shadowDistance " << document.renderSettings.shadowDistance << '\n'
           << "outlinePixels " << document.renderSettings.outline.silhouetteWidthPixels << '\n'
           << "antiAliasing " << document.renderSettings.antiAliasing << '\n'
           << "sky " << static_cast<unsigned>(document.renderSettings.sky.mode) << ' '
           << document.renderSettings.sky.color[0] << ' ' << document.renderSettings.sky.color[1] << ' ' << document.renderSettings.sky.color[2] << ' '
           << document.renderSettings.sky.intensity << ' ' << document.renderSettings.sky.rotationDegrees << '\n'
           << "pathTracing " << document.renderSettings.pathTraceSamples << ' ' << document.renderSettings.pathTraceBounces << '\n'
           << "pathTracingPreview " << document.renderSettings.pathTraceDenoise << ' ' << document.renderSettings.pathTraceSeed << '\n'
           << "sceneLighting " << std::boolalpha << document.renderSettings.sceneLightingEnabled << '\n'
           << "environmentSpecularModel " << std::quoted(std::string(environmentSpecularModelName(document.renderSettings.environmentSpecularModel))) << '\n'
           << "screenSpaceReflection " << std::quoted(encodeScreenSpaceReflection(document.renderSettings.screenSpaceReflection).dump()) << '\n'
           << "nprLighting " << std::quoted(encodeNprLighting(document.renderSettings.nprLighting).dump()) << '\n'
           << "innerOutlineEnabled " << std::boolalpha
           << document.renderSettings.innerOutlineEnabled << '\n'
           << "outlineStrength " << document.renderSettings.outline.strength << '\n'
           << "gradeExposureEv " << document.renderSettings.grade.exposureEv << '\n'
           << "gradeLut " << std::quoted(encodeLevelRenderSettings(document.renderSettings).at("gradeLut").dump()) << '\n'
           << "characterBackgroundEnabled " << std::boolalpha
           << document.renderSettings.characterPresentation.backgroundEnabled << '\n'
           << "characterPlatformEnabled " << std::boolalpha
           << document.renderSettings.characterPresentation.platformEnabled << '\n'
           << "blackholeQuality "
           << blackholeQualityName(document.renderSettings.blackhole.quality) << '\n'
           << "blackholeCamera "
           << blackholeCameraName(document.renderSettings.blackhole.camera) << '\n'
           << "sceneRenderer "
           << sceneTypeName(document.renderSettings.sceneType) << '\n';
}

}  // namespace

SceneDocument SceneDocument::fromAsset(
    const std::filesystem::path& assetPath) {
    if (assetPath.empty()) {
        throw std::invalid_argument("Scene asset path must not be empty");
    }
    SceneDocument document;
    document.sceneId = assetPath.stem().u8string();
    document.resources.push_back({"asset-0", "gltf", assetPath});
    SceneNode root;
    root.id = "root";
    root.name = document.sceneId;
    root.resourceId = "asset-0";
    document.nodes.push_back(std::move(root));
    return document;
}

SceneDocument SceneDocument::load(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("Unable to open scene: " + path.u8string());
    }
    requireHeader(input, "AzureRender Scene v1");
    SceneDocument document;
    std::uint32_t schemaVersion = 0;
    std::size_t resourceCount = 0;
    std::size_t nodeCount = 0;
    std::string key;
    if (!(input >> key >> schemaVersion) || key != "schemaVersion"
        || schemaVersion < 1 || schemaVersion > kSchemaVersion) {
        throw std::runtime_error("Unsupported .azscene schemaVersion");
    }
    std::uint32_t renderSettingsVersion = 1;
    if (!(input >> key)) {
        throw std::runtime_error("Missing .azscene sceneId");
    }
    if (key == "renderSettingsVersion") {
        if (!(input >> renderSettingsVersion >> key)) {
            throw std::runtime_error(
                "Invalid .azscene renderSettingsVersion");
        }
    }
    if (key != "sceneId" || !(input >> std::quoted(document.sceneId))) {
        throw std::runtime_error("Missing .azscene sceneId");
    }
    if (!(input >> key >> resourceCount) || key != "resourceCount") {
        throw std::runtime_error("Missing .azscene resourceCount");
    }
    for (std::size_t index = 0; index < resourceCount; ++index) {
        SceneResource resource;
        std::string resourcePath;
        if (!(input >> key >> std::quoted(resource.id)
              >> std::quoted(resource.type) >> std::quoted(resourcePath))
            || key != "resource") {
            throw std::runtime_error("Invalid .azscene resource");
        }
        resource.path = std::filesystem::u8path(resourcePath);
        document.resources.push_back(std::move(resource));
    }
    if (!(input >> key >> nodeCount) || key != "nodeCount") {
        throw std::runtime_error("Missing .azscene nodeCount");
    }
    for (std::size_t index = 0; index < nodeCount; ++index) {
        SceneNode node;
        if (!(input >> key >> std::quoted(node.id)
              >> std::quoted(node.name) >> std::quoted(node.parentId)
              >> std::quoted(node.resourceId)) || key != "node") {
            throw std::runtime_error("Invalid .azscene node");
        }
        node.visible = readBool(input, "visible");
        if (schemaVersion >= 2) {
            if (!(input >> key
                  >> node.translation[0] >> node.translation[1]
                  >> node.translation[2] >> node.rotation[0]
                  >> node.rotation[1] >> node.rotation[2]
                  >> node.scale[0] >> node.scale[1] >> node.scale[2])
                || key != "transform") {
                throw std::runtime_error("Invalid .azscene node transform");
            }
            if (!(input >> key >> std::quoted(node.prefabSource)
                  >> std::quoted(node.instanceOf)) || key != "prefab") {
                throw std::runtime_error("Invalid .azscene prefab instance");
            }
        }
        if(schemaVersion>=4){
            std::size_t count=0;if(!(input>>key>>count)||key!="materialSlotCount"||count>256)throw std::invalid_argument("Invalid material slot count");
            for(std::size_t slotIndex=0;slotIndex<count;++slotIndex){std::string slot,asset;if(!(input>>key>>std::quoted(slot)>>std::quoted(asset))||key!="materialSlot"||slot.empty()||asset.empty()||!node.materialSlots.emplace(slot,asset).second)throw std::invalid_argument("Invalid scene material slot");}
        }
        if(schemaVersion>=13){
            std::string encoded;
            if(!(input>>key>>std::quoted(encoded))||key!="objectShadow")throw std::invalid_argument("Missing scene object shadow field");
            const auto value=Json::parse(encoded);
            if(!value.is_null())node.objectShadow=decodeObjectShadowComponent(value);
        }
        document.nodes.push_back(std::move(node));
    }
    if (schemaVersion >= 3) {
        std::size_t lightCount = 0;
        if (!(input >> key >> lightCount) || key != "lightCount") {
            throw std::runtime_error("Missing .azscene lightCount");
        }
        document.lights.reserve(lightCount);
        for (std::size_t index = 0; index < lightCount; ++index) {
            SceneLight light;
            if (!(input >> key >> std::quoted(light.id)
                  >> std::quoted(light.nodeId)
                  >> light.color[0] >> light.color[1] >> light.color[2]
                  >> light.intensity >> light.radius >> std::boolalpha
                  >> light.enabled)
                || key != "light") {
                throw std::runtime_error("Invalid .azscene light");
            }
            if (light.id.empty() || light.nodeId.empty()
                || !(light.radius > 0.0F) || !(light.intensity >= 0.0F)) {
                throw std::runtime_error("Invalid .azscene light values");
            }
            if (schemaVersion >= 5) {
                int type = 0;
                if (!(input >> key >> type >> light.shape.direction[0]
                    >> light.shape.direction[1] >> light.shape.direction[2]
                    >> light.shape.innerCone >> light.shape.outerCone) || key != "lightShape")
                    throw std::runtime_error("Invalid .azscene light shape");
                light.shape.type = static_cast<LightKind>(type);
                if(schemaVersion>=8) {
                    int falloff=0;
                    if(!(input>>key>>falloff>>light.shape.falloffExponent)||key!="lightFalloff")
                        throw std::runtime_error("Invalid .azscene light falloff");
                    light.shape.falloff=static_cast<LightFalloff>(falloff);
                }
                if(schemaVersion>=9) {
                    std::string payload;
                    if(!(input>>key>>std::quoted(payload))||key!="lightInfluenceBox")
                        throw std::runtime_error("Invalid .azscene light influence box");
                    light.shape.influenceBox=nlohmann::json::parse(payload).get<LightInfluenceBox>();
                }
                if(schemaVersion>=10) {
                    if(!(input>>key>>light.shape.specularScale)||key!="lightSpecularScale")
                        throw std::runtime_error("Invalid .azscene light specular scale");
                }
                if(schemaVersion>=11) {
                    std::string payload;
                    if(!(input>>key>>std::quoted(payload))||key!="lightReceivers")
                        throw std::runtime_error("Invalid .azscene light receivers");
                    light.shape.receivers=nlohmann::json::parse(payload).get<LightReceivers>();
                }
                if(schemaVersion>=12) {
                    std::string payload;
                    if(!(input>>key>>std::quoted(payload))||key!="lightCharacterResponse")
                        throw std::runtime_error("Invalid .azscene character light response");
                    const auto value=nlohmann::json::parse(payload);
                    if(!value.is_null())light.shape.characterResponse=value.get<LightShape::CharacterResponse>();
                }
                light.shape.validate();
            }
            document.lights.push_back(std::move(light));
        }
    }
    if(schemaVersion>=6){
        std::string payload;
        if(!(input>>key>>std::quoted(payload))||key!="reflectionProbe")throw std::runtime_error("Missing reflection probe");
        document.reflectionProbe=nlohmann::json::parse(payload).get<ReflectionProbe>();
    }
    if(schemaVersion>=7){
        std::string payload;
        if(!(input>>key>>std::quoted(payload))||key!="irradianceVolume")throw std::runtime_error("Missing irradiance volume");
        document.irradianceVolume=nlohmann::json::parse(payload).get<IrradianceVolumeBinding>();
    }
    if(schemaVersion>=14){
        std::string payload;
        if(!(input>>key>>std::quoted(payload))||key!="viewFog")throw std::runtime_error("Missing view fog binding");
        document.viewFog=nlohmann::json::parse(payload).get<ViewFogBinding>();
    }
    if (!(input >> key >> document.renderSettings.showcasePreset)
        || key != "showcasePreset") {
        throw std::runtime_error("Missing .azscene showcasePreset");
    }
    if (!(input >> key >> document.renderSettings.styleMaskStrength)
        || key != "styleMaskStrength") {
        throw std::runtime_error("Missing .azscene styleMaskStrength");
    }
    if (!(input >> key >> document.renderSettings.diffuseBandThreshold)
        || key != "diffuseBandThreshold") {
        throw std::runtime_error("Missing .azscene diffuseBandThreshold");
    }
    if (renderSettingsVersion >= 7) {
        if (!(input >> key
              >> document.renderSettings.shadow.maximumFilterRadiusTexels)
            || key != "shadowMaximumFilterRadiusTexels") {
            throw std::runtime_error(
                "Missing .azscene shadowMaximumFilterRadiusTexels");
        }
    }
    if (renderSettingsVersion >= 8) {
        if (!(input >> key >> document.renderSettings.cameraNear) || key!="cameraNear"
            || !(input >> key >> document.renderSettings.cameraFar) || key!="cameraFar"
            || !(input >> key >> document.renderSettings.shadowDistance) || key!="shadowDistance")
            throw std::runtime_error("Invalid .azscene camera or shadow range");
    }
    if(renderSettingsVersion>=9 && (!(input>>key>>document.renderSettings.outline.silhouetteWidthPixels)||key!="outlinePixels"))
        throw std::runtime_error("Missing .azscene outlinePixels");
    if(renderSettingsVersion>=9 && (!(input>>key>>document.renderSettings.antiAliasing)||key!="antiAliasing"))
        throw std::runtime_error("Missing .azscene antiAliasing");
    if(renderSettingsVersion>=10){
        unsigned mode=0;
        auto& sky=document.renderSettings.sky;
        if(!(input>>key>>mode>>sky.color[0]>>sky.color[1]>>sky.color[2]>>sky.intensity>>sky.rotationDegrees)||key!="sky"||mode>2)
            throw std::runtime_error("Invalid .azscene sky");
        sky.mode=static_cast<SkyMode>(mode);
    }
    if(renderSettingsVersion>=11 && (!(input>>key>>document.renderSettings.pathTraceSamples>>document.renderSettings.pathTraceBounces)||key!="pathTracing"))
        throw std::runtime_error("Invalid .azscene pathTracing");
    if(renderSettingsVersion>=12 && (!(input>>key>>document.renderSettings.pathTraceDenoise>>document.renderSettings.pathTraceSeed)||key!="pathTracingPreview"))
        throw std::runtime_error("Invalid .azscene pathTracingPreview");
    if(renderSettingsVersion>=13 && (!(input>>key>>std::boolalpha>>document.renderSettings.sceneLightingEnabled)||key!="sceneLighting"))
        throw std::runtime_error("Invalid .azscene sceneLighting");
    if(renderSettingsVersion>=15){
        std::string model;
        if(!(input>>key>>std::quoted(model))||key!="environmentSpecularModel")
            throw std::runtime_error("Invalid .azscene environmentSpecularModel");
        document.renderSettings.environmentSpecularModel=environmentSpecularModelFromString(model);
    }
    if(renderSettingsVersion>=16){
        std::string serialized;
        if(!(input>>key>>std::quoted(serialized))||key!="screenSpaceReflection")
            throw std::runtime_error("Invalid .azscene screenSpaceReflection");
        decodeScreenSpaceReflection(document.renderSettings.screenSpaceReflection,nlohmann::json::parse(serialized));
    }
    if(renderSettingsVersion>=17){
        std::string serialized;
        if(!(input>>key>>std::quoted(serialized))||key!="nprLighting")throw std::runtime_error("Invalid .azscene nprLighting");
        decodeNprLighting(document.renderSettings.nprLighting,nlohmann::json::parse(serialized));
    }
    if (!(input >> key >> std::boolalpha
          >> document.renderSettings.innerOutlineEnabled)
        || key != "innerOutlineEnabled") {
        throw std::runtime_error("Missing .azscene innerOutlineEnabled");
    }
    if (!(input >> key >> document.renderSettings.outline.strength)
        || key != "outlineStrength") {
        throw std::runtime_error("Missing .azscene outlineStrength");
    }
    if (!(input >> key >> document.renderSettings.grade.exposureEv)
        || key != "gradeExposureEv") {
        throw std::runtime_error("Missing .azscene gradeExposureEv");
    }
    // Blackhole settings and sceneRenderer are optional for backward
    if(renderSettingsVersion>=14){
        std::string serialized;
        if(!(input>>key>>std::quoted(serialized))||key!="gradeLut")throw std::runtime_error("Missing .azscene gradeLut");
        decodeLevelRenderSettings(document.renderSettings,{{"gradeLut",nlohmann::json::parse(serialized)}});
    }
    // compatibility with schema v1 documents.
    // documents that predate the pluggable scene renderer.
    if (!(input >> key)) {
        // Reached end of file: keep the default Character renderer.
        document.renderSettings.sceneType = SceneType::Character;
    } else {
        if (key == "characterBackgroundEnabled") {
            if (!(input >> std::boolalpha
                  >> document.renderSettings.characterPresentation.backgroundEnabled
                  >> key)) {
                throw std::runtime_error(
                    "Invalid .azscene characterBackgroundEnabled");
            }
        }
        if (key == "characterPlatformEnabled") {
            if (!(input >> std::boolalpha
                  >> document.renderSettings.characterPresentation.platformEnabled
                  >> key)) {
                throw std::runtime_error(
                    "Invalid .azscene characterPlatformEnabled");
            }
        }
        if (key == "blackholeQuality") {
            std::string value;
            if (!(input >> value)) {
                throw std::runtime_error("Invalid .azscene blackholeQuality");
            }
            if (value == "performance") {
                document.renderSettings.blackhole.quality = BlackholeQuality::Performance;
            } else if (value == "balanced") {
                document.renderSettings.blackhole.quality = BlackholeQuality::Balanced;
            } else if (value != "cinematic") {
                throw std::runtime_error("Unsupported .azscene blackholeQuality: " + value);
            }
            if (!(input >> key)) {
                throw std::runtime_error("Missing .azscene blackholeCamera");
            }
        }
        if (key == "blackholeCamera") {
            std::string value;
            if (!(input >> value)) {
                throw std::runtime_error("Invalid .azscene blackholeCamera");
            }
            if (value == "orbit-left") {
                document.renderSettings.blackhole.camera = BlackholeCamera::OrbitLeft;
            } else if (value == "high") {
                document.renderSettings.blackhole.camera = BlackholeCamera::High;
            } else if (value == "close") {
                document.renderSettings.blackhole.camera = BlackholeCamera::Close;
            } else if (value == "over-shoulder") {
                document.renderSettings.blackhole.camera = BlackholeCamera::OverShoulder;
            } else if (value != "front") {
                throw std::runtime_error("Unsupported .azscene blackholeCamera: " + value);
            }
            if (!(input >> key)) {
                throw std::runtime_error("Missing .azscene sceneRenderer");
            }
        }
        if (key != "sceneRenderer") {
            throw std::runtime_error("Invalid .azscene trailing field: " + key);
        }
        std::string sceneTypeValue;
        if (!(input >> sceneTypeValue)) {
            throw std::runtime_error("Invalid .azscene sceneRenderer");
        }
        try {
            document.renderSettings.sceneType =
                sceneTypeFromString(sceneTypeValue);
        } catch (const std::invalid_argument&) {
            throw std::runtime_error(
                "Unsupported .azscene sceneRenderer: " + sceneTypeValue);
        }
    }
    document.renderSettings = migrateRenderSettings(
        document.renderSettings, renderSettingsVersion);
    auto& lut=document.renderSettings.grade.lut;
    if(!lut.asset.empty()){
        static const std::regex assetId("[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}");
        const auto lutPath=std::filesystem::u8path(lut.asset);
        const bool projectReference=!lutPath.is_absolute()
            &&(lut.asset.find(":/")!=std::string::npos||std::regex_match(lut.asset,assetId));
        if(!projectReference)resolveColorGradingLutSettings(lut,nullptr,path.parent_path());
    }
    return document;
}

void SceneDocument::save(const std::filesystem::path& path) const {
    validateRenderSettings(renderSettings);
    if (path.empty()) {
        throw std::invalid_argument("Scene save path must not be empty");
    }
    const std::filesystem::path directory =
        path.parent_path().empty() ? std::filesystem::path(".")
                                   : path.parent_path();
    std::error_code directoryError;
    if (!std::filesystem::is_directory(directory, directoryError)
        || directoryError) {
        throw std::runtime_error(
            "Scene save directory does not exist: " + directory.u8string());
    }

    // Atomic save: write a unique temporary file in the same directory, flush
    // and close it, then rename over the target. A failure at any stage leaves
    // the original target untouched and removes the temporary file.
    static std::mt19937_64 generator{
        static_cast<std::uint64_t>(std::chrono::steady_clock::now()
                                      .time_since_epoch().count())};
    const std::string suffix =
        ".tmp." + std::to_string(generator());
    const std::filesystem::path temporary =
        directory / std::filesystem::u8path(path.filename().u8string() + suffix);

    try {
        {
            std::ofstream output(temporary,
                std::ios::binary | std::ios::trunc);
            if (!output) {
                throw std::runtime_error(
                    "Unable to write scene: " + temporary.u8string());
            }
            writeDocument(output, *this);
            output.flush();
            if (!output) {
                throw std::runtime_error(
                    "Failed to flush scene: " + temporary.u8string());
            }
        }
        std::error_code renameError;
        std::filesystem::rename(temporary, path, renameError);
        if (renameError) {
            throw std::runtime_error(
                "Unable to replace scene: " + path.u8string()
                + " (" + renameError.message() + ")");
        }
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        throw;
    }
}

}  // namespace azurerender
