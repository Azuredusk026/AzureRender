#include "assets/GltfLoader.hpp"
#include "runtime/LevelRenderSettings.hpp"
#include <nlohmann/json.hpp>
#include <stb_image_write.h>
#include <fstream>
#include <iostream>
#include <cmath>
#include <chrono>
using namespace azurerender;
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }

void testCharacterProfileTextures(const nlohmann::json& fixture, const std::filesystem::path& directory) {
    using Json = nlohmann::json;
    const std::vector<std::uint8_t> rampPixels{17,64,128,223, 96,32,224,160, 29,83,149,215, 255,0,1,254};
    const std::vector<std::uint8_t> hairPixels{32,96,160,224, 96,32,224,160, 17,83,149,215, 255,0,1,254};
    require(stbi_write_png((directory/"diffuse-ramp.png").string().c_str(),4,1,4,rampPixels.data(),16)!=0,
        "Cannot write diffuse ramp fixture");
    require(stbi_write_png((directory/"hair-data.png").string().c_str(),2,2,4,hairPixels.data(),8)!=0,
        "Cannot write packed hair fixture");
    require(stbi_write_png((directory/"single-ramp.png").string().c_str(),1,1,4,rampPixels.data(),4)!=0,
        "Cannot write undersized ramp fixture");
    auto source=fixture;
    const auto addTexture=[&](const char* uri) {
        const int image=static_cast<int>(source["images"].size());
        source["images"].push_back({{"uri",uri}});
        const int texture=static_cast<int>(source["textures"].size());
        source["textures"].push_back({{"source",image}});
        return texture;
    };
    const int ramp=addTexture("diffuse-ramp.png");
    const int hair=addTexture("hair-data.png");
    const int single=addTexture("single-ramp.png");
    auto& profile=source["materials"][0]["extras"]["azureRenderMaterial"];
    profile["features"]={"hair-anisotropy","diffuse-ramp","silhouette-outline"};
    profile["diffuseRamp"]={{"schemaVersion",1},{"texture",ramp}};
    profile["hair"]["dataTexture"]=hair;
    const auto load=[&](const Json& document) {
        const auto path=directory/"character-profile.gltf";
        std::ofstream(path)<<document.dump();
        return loadGltfAsset(path.string());
    };
    const auto reject=[&](const Json& document,const char* diagnostic,const char* description) {
        std::string failure;
        try{(void)load(document);}catch(const std::exception& error){failure=error.what();}
        if(failure.empty()||failure.find(diagnostic)==std::string::npos)
            throw std::runtime_error(std::string(description)+"; actual diagnostic: "+failure);
    };

    const auto asset=load(source);
    const auto& actual=asset.materials.at(0);
    require(actual.styleMaskWidth==4&&actual.styleMaskHeight==1&&actual.styleMaskPixels==rampPixels,
        "Diffuse ramp must preserve every linear RGBA byte and horizontal dimension");
    require(actual.textureSettings[4].colorSpace=="linear"&&actual.textureSettings[4].mipPolicy=="none"
        &&actual.textureSettings[4].wrapU=="clamp"&&actual.textureSettings[4].wrapV=="clamp",
        "Diffuse ramp requires linear sampling, clamped coordinates and no mip reduction");
    require(actual.hairDataWidth==2&&actual.hairDataHeight==2&&actual.hairDataPixels==hairPixels,
        "Packed hair data must preserve all four linear channels by default");
    require(actual.textureSettings[6].colorSpace=="linear", "Packed hair texture must use linear sampling");
    require((actual.materialFeatures&MaterialFeatureDiffuseRamp)!=0
        &&(actual.materialFeatures&MaterialFeatureSilhouetteOutline)!=0,
        "Explicit diffuse ramp and silhouette features must survive production loading");
    auto negative=source;
    negative["materials"][0]["extras"]["azureRenderMaterial"]["hair"]["dataNormalY"]="negative";
    require(load(negative).materials[0].hairDataPixels==hairPixels,
        "Negative-Y packed hair data must retain the source bytes");
    auto positive=source;
    positive["materials"][0]["extras"]["azureRenderMaterial"]["hair"]["dataNormalY"]="positive";
    auto positivePixels=hairPixels;
    for(std::size_t pixel=0;pixel<positivePixels.size();pixel+=4) {
        positivePixels[pixel+1]=static_cast<std::uint8_t>(255-positivePixels[pixel+1]);
        positivePixels[pixel+3]=static_cast<std::uint8_t>(255-positivePixels[pixel+3]);
    }
    require(load(positive).materials[0].hairDataPixels==positivePixels,
        "Positive-Y packed hair data must invert both packed normal Y channels only");

    for(const auto& badIndex:Json::array({-1,.5,"invalid",100000})) {
        auto invalid=source;
        invalid["materials"][0]["extras"]["azureRenderMaterial"]["diffuseRamp"]["texture"]=badIndex;
        reject(invalid,"Invalid character texture", "Invalid diffuse ramp texture indices must be rejected");
        invalid=source;
        invalid["materials"][0]["extras"]["azureRenderMaterial"]["hair"]["dataTexture"]=badIndex;
        reject(invalid,"Invalid character texture", "Invalid packed hair texture indices must be rejected");
    }
    for(const int badShape:{hair,single}) {
        auto invalid=source;
        invalid["materials"][0]["extras"]["azureRenderMaterial"]["diffuseRamp"]["texture"]=badShape;
        reject(invalid,"horizontal single-row", "Diffuse ramp must reject square and single-sample images");
    }
    auto invalid=source;
    invalid["materials"][0]["extras"]["azureRenderMaterial"]["features"]={"hair-anisotropy"};
    reject(invalid,"Invalid diffuseRamp profile", "Diffuse ramp profile requires its declared feature");
    invalid=source;
    invalid["materials"][0]["extras"]["azureRenderMaterial"].erase("diffuseRamp");
    reject(invalid,"diffuse-ramp requires a profile", "Diffuse ramp feature requires its typed input");
    for(const auto& version:Json::array({0,2,.5,"1"})) {
        invalid=source;
        invalid["materials"][0]["extras"]["azureRenderMaterial"]["diffuseRamp"]["schemaVersion"]=version;
        reject(invalid,"Invalid diffuseRamp profile", "Unsupported diffuse ramp versions must be rejected");
    }
    invalid=source;
    invalid["materials"][0]["extras"]["azureRenderMaterial"]["diffuseRamp"]["unknown"]=true;
    reject(invalid,"Unknown diffuseRamp field", "Unknown diffuse ramp fields must be rejected");
    invalid=source;
    invalid["materials"][0]["extras"]["azureRenderMaterial"]["diffuseRamp"].erase("texture");
    reject(invalid,"Invalid character texture", "Diffuse ramp requires a texture index");
    for(const int texture:{ramp,hair}) {
        invalid=source;
        invalid["textures"][texture]["source"]=-1;
        reject(invalid,"Character texture has no image", "Character data textures require a resolved image");
    }
    invalid=source;
    invalid["materials"][0]["extras"]["azureRenderMaterial"]["class"]="face";
    reject(invalid,"Invalid diffuseRamp profile", "Face materials reserve their independent ramp binding");
    invalid=source;
    invalid["materials"][0]["extras"]["azureRenderMaterial"]["faceSdf"]={{"schemaVersion",2}};
    reject(invalid,"Invalid diffuseRamp profile", "Diffuse ramp and face SDF must reject a shared texture-slot conflict");
    invalid=source;
    invalid["materials"][0]["extras"]["azureRenderMaterial"]["features"]={"diffuse-ramp"};
    reject(invalid,"requires hair anisotropy", "Packed hair input requires hair anisotropy");
    invalid=source;
    invalid["materials"][0]["extras"]["azureRenderMaterial"]["class"]="fabric";
    reject(invalid,"requires hair anisotropy", "Packed hair input requires the hair material class");
    for(const auto& direction:Json::array({"up",false,1})) {
        invalid=source;
        invalid["materials"][0]["extras"]["azureRenderMaterial"]["hair"]["dataNormalY"]=direction;
        reject(invalid,"hair.dataNormalY", "Packed hair Y direction must be an explicit supported string");
    }
    invalid=positive;
    invalid["materials"][0]["extras"]["azureRenderMaterial"]["hair"].erase("dataTexture");
    reject(invalid,"hair.dataNormalY", "Packed hair Y direction requires its source texture");
}

void testProjectedHairRamp(const nlohmann::json& fixture,const std::filesystem::path& directory) {
    using Json=nlohmann::json;
    const std::vector<std::uint8_t> pixels{17,63,107,211, 29,79,137,227, 43,97,151,239, 59,113,179,251};
    require(stbi_write_png((directory/"projected-ramp.png").string().c_str(),2,2,4,pixels.data(),8)!=0,
        "Cannot write two-dimensional hair fixture");
    auto source=fixture;
    const int image=static_cast<int>(source["images"].size());source["images"].push_back({{"uri","projected-ramp.png"}});
    const int texture=static_cast<int>(source["textures"].size());source["textures"].push_back({{"source",image}});
    auto& profile=source["materials"][0]["extras"]["azureRenderMaterial"];
    profile={{"schemaVersion",2},{"class","hair"},{"features",{"hair-anisotropy","hair-projected-ramp"}},
        {"hair",{{"power",200},{"strength",1},{"rampRow",3},{"strandShift",0},{"dataTexture",texture},
            {"projectedRamp",{{"schemaVersion",1},{"parameterTexture",texture},{"rampTexture",texture},
                {"primaryShift",-.02},{"secondaryShift",.02},{"intensity",1.3},{"secondaryRange",.5},
                {"edgeFade",5},{"specularNormalScale",1.4},{"surfaceNormalScale",.8},{"directionX",.25},
                {"secondaryColor",{.6,.1,.2}},{"objectBasis",{0,0,-1,0,1,0,1,0,0}}}}}}};
    const auto load=[&](const Json& document) {
        const auto path=directory/"projected-hair.gltf";std::ofstream(path)<<document.dump();return loadGltfAsset(path.string());
    };
    const auto reject=[&](const Json& document,const char* diagnostic="projectedRamp") {
        std::string error;try{(void)load(document);}catch(const std::exception& failure){error=failure.what();}
        if(error.find(diagnostic)==std::string::npos)throw std::runtime_error("Projected hair invalid input accepted: "+error
            +"; "+document["materials"][0]["extras"]["azureRenderMaterial"]["hair"].dump());
    };
    const auto asset=load(source);const auto& material=asset.materials.at(0);const auto& hair=material.projectedHairRamp;
    require(hair.present&&hair.parameterPixels==pixels&&hair.parameterWidth==2&&hair.parameterHeight==2,
        "Projected hair must retain all four parameter channels independently from ORM");
    require(material.matcapWidth==2&&material.matcapHeight==2&&material.matcapPixels==pixels,
        "Projected hair must preserve both ramp dimensions and every linear byte");
    require(material.textureSettings[5].colorSpace=="linear"&&material.textureSettings[5].wrapU=="clamp"
        &&material.textureSettings[5].wrapV=="clamp"&&material.textureSettings[5].mipPolicy=="none"
        &&material.textureSettings[5].anisotropy==1&&material.textureSettings[7].colorSpace=="linear",
        "Projected ramp and parameter sampling must retain their independent linear semantics");
    require(std::abs(hair.primaryShift+.02F)<.000001F&&std::abs(hair.secondaryShift-.02F)<.000001F
        &&std::abs(hair.specularNormalScale-1.4F)<.000001F&&hair.objectBasis[2]==-1,
        "Signed shifts, independent normal scales and rotated source basis must survive loading");
    auto strand=source;
    auto& strandProfile=strand["materials"][0]["extras"]["azureRenderMaterial"]["hair"]["projectedRamp"];
    strandProfile["surfaceResponse"]="strand-diffuse";strandProfile["diffuseScale"]=.96;
    const auto strandAsset=load(strand);
    require(strandAsset.materials[0].projectedHairRamp.surfaceResponse==AssetMaterial::ProjectedHairRamp::SurfaceResponse::StrandDiffuse
        &&std::abs(strandAsset.materials[0].projectedHairRamp.diffuseScale-.96F)<.000001F,
        "Strand surface response and independent diffuse energy must survive loading");
    require(hair.surfaceResponse==AssetMaterial::ProjectedHairRamp::SurfaceResponse::Layered&&hair.diffuseScale==1,
        "Projected ramp defaults must retain the layered surface contract");
    strandProfile["surfaceResponse"]="layered";(void)load(strand);
    for(const auto& mode:Json::array({"unknown",1,true,nullptr})) {
        auto invalid=strand;invalid["materials"][0]["extras"]["azureRenderMaterial"]["hair"]["projectedRamp"]["surfaceResponse"]=mode;
        reject(invalid,"projectedRamp.surfaceResponse");
    }
    for(const auto& scale:Json::array({-1,1.1,".96",true,nullptr})) {
        auto invalid=strand;invalid["materials"][0]["extras"]["azureRenderMaterial"]["hair"]["projectedRamp"]["diffuseScale"]=scale;
        reject(invalid,"projectedRamp.diffuseScale");
    }
    for(const auto& version:Json::array({0,2,.5,"1"})) {
        auto bad=source;bad["materials"][0]["extras"]["azureRenderMaterial"]["hair"]["projectedRamp"]["schemaVersion"]=version;reject(bad);
    }
    for(const char* field:{"primaryShift","secondaryShift","intensity","secondaryRange","edgeFade",
        "specularNormalScale","surfaceNormalScale","directionX","secondaryColor","objectBasis","parameterTexture","rampTexture"}) {
        auto bad=source;bad["materials"][0]["extras"]["azureRenderMaterial"]["hair"]["projectedRamp"].erase(field);reject(bad);
    }
    for(const char* field:{"primaryShift","secondaryShift","intensity","secondaryRange","edgeFade",
        "specularNormalScale","surfaceNormalScale","directionX"}) {
        auto bad=source;bad["materials"][0]["extras"]["azureRenderMaterial"]["hair"]["projectedRamp"][field]=1000;reject(bad);
    }
    for(const auto& index:Json::array({-1,.5,"1",99999}))for(const char* field:{"parameterTexture","rampTexture"}) {
        auto bad=source;bad["materials"][0]["extras"]["azureRenderMaterial"]["hair"]["projectedRamp"][field]=index;reject(bad);
    }
    for(const char* conflict:{"hair-ramp","face-sdf-eligible","scene-tint","overlay","brow-overlay"}) {
        auto bad=source;bad["materials"][0]["extras"]["azureRenderMaterial"]["features"].push_back(conflict);reject(bad);
    }
    auto bad=source;bad["materials"][0]["extras"]["azureRenderMaterial"]["features"]={"hair-anisotropy"};reject(bad);
    bad=source;bad["materials"][0]["extras"]["azureRenderMaterial"]["hair"].erase("projectedRamp");reject(bad);
    bad=source;bad["materials"][0]["extras"]["azureRenderMaterial"]["hair"]["projectedRamp"]["unknown"]=1;reject(bad);
    bad=source;bad["materials"][0]["extras"]["azureRenderMaterial"]["hair"]["projectedRamp"]["objectBasis"]={1,0,0,1,0,0,0,0,1};reject(bad);
    bad=source;bad["images"][image]["uri"]="diffuse-ramp.png";reject(bad);
    bad=source;bad["textures"][texture]["source"]=-1;reject(bad,"Character texture has no image");
}

void testMultiplyComposite(const nlohmann::json& fixture, const std::filesystem::path& directory) {
    using Json=nlohmann::json;
    auto source=fixture;
    auto& material=source["materials"][0];
    material["alphaMode"]="BLEND";
    material["extras"]["azureRenderMaterial"]={{"schemaVersion",2},{"class","overlay"},
        {"features",{"overlay","multiply-composite"}},
        {"composite",{{"schemaVersion",1},{"mode","multiply"}}}};
    const auto load=[&](const Json& document) {
        const auto path=directory/"multiply-composite.gltf";
        std::ofstream(path)<<document.dump();
        return loadGltfAsset(path.string());
    };
    const auto reject=[&](const Json& document) {
        std::string failure;
        try{(void)load(document);}catch(const std::exception& error){failure=error.what();}
        require(failure.find("composite")!=std::string::npos,
            "Invalid multiply composite must be rejected with its contract diagnostic");
    };
    const auto overlay=load(source).materials.at(0);
    require(overlay.alphaMode==AssetAlphaMode::Blend
        &&(overlay.materialFeatures&MaterialFeatureMultiplyComposite)!=0,
        "Multiply composite must retain its explicit transparent feature");
    auto generic=source;
    generic["materials"][0]["extras"]["azureRenderMaterial"]["class"]="generic";
    generic["materials"][0]["extras"]["azureRenderMaterial"]["features"]={"multiply-composite"};
    require(load(generic).materials.at(0).materialClass==AssetMaterialClass::Generic,
        "Multiply filters must also support generic content");
    for(const auto& version:Json::array({0,2,.5,"1"})) {
        auto invalid=source;
        invalid["materials"][0]["extras"]["azureRenderMaterial"]["composite"]["schemaVersion"]=version;
        reject(invalid);
    }
    for(const auto& version:Json::array({1,2.5})) {
        auto invalid=source;
        invalid["materials"][0]["extras"]["azureRenderMaterial"]["schemaVersion"]=version;
        reject(invalid);
    }
    for(const auto& mode:Json::array({"alpha-over",false,1})) {
        auto invalid=source;
        invalid["materials"][0]["extras"]["azureRenderMaterial"]["composite"]["mode"]=mode;
        reject(invalid);
    }
    for(const char* mode:{"OPAQUE","MASK"}) {
        auto invalid=source;invalid["materials"][0]["alphaMode"]=mode;reject(invalid);
    }
    for(const char* category:{"hair","face","fabric"}) {
        auto invalid=source;
        invalid["materials"][0]["extras"]["azureRenderMaterial"]["class"]=category;
        reject(invalid);
    }
    for(const char* feature:{"scene-tint","scene-refraction","brow-overlay","hair-anisotropy"}) {
        auto invalid=source;
        invalid["materials"][0]["extras"]["azureRenderMaterial"]["features"].push_back(feature);
        reject(invalid);
    }
    auto invalid=source;
    invalid["materials"][0]["extras"]["azureRenderMaterial"].erase("composite");reject(invalid);
    invalid=source;
    invalid["materials"][0]["extras"]["azureRenderMaterial"]["features"]={"overlay"};reject(invalid);
    invalid=source;
    invalid["materials"][0]["extras"]["azureRenderMaterial"]["composite"]["unknown"]=true;reject(invalid);
    invalid=source;
    invalid["materials"][0]["extras"]["azureRenderMaterial"]["composite"]=false;reject(invalid);
    for(const char* field:{"schemaVersion","mode"}) {
        invalid=source;
        invalid["materials"][0]["extras"]["azureRenderMaterial"]["composite"].erase(field);reject(invalid);
    }
}

void testUnlitSurface(const nlohmann::json& fixture,const std::filesystem::path& directory) {
    using Json=nlohmann::json;
    auto source=fixture;
    auto& material=source["materials"][0];
    material["extras"]={{"azureRenderMaterial",{{"schemaVersion",2},{"class","emissive"},
        {"features",{"emissive-mask","unlit-surface"}},
        {"surface",{{"schemaVersion",1},{"lighting","unlit"}}}}}};
    material.erase("extensions");
    material["emissiveFactor"]={.1,.2,.3};
    const auto path=directory/"unlit-surface.gltf";
    const auto load=[&](const Json& doc){std::ofstream(path)<<doc.dump();return loadGltfAsset(path.string());};
    const auto reject=[&](const Json& doc){
        std::string failure;
        try{(void)load(doc);}catch(const std::exception& error){failure=error.what();}
        require(failure.find("unlit surface")!=std::string::npos,
            "Invalid unlit surface requires its own diagnostic");
    };
    for(const char* category:{"generic","emissive"})for(const char* alpha:{"OPAQUE","MASK","BLEND"}) {
        auto doc=source;doc["materials"][0]["extras"]["azureRenderMaterial"]["class"]=category;
        doc["materials"][0]["alphaMode"]=alpha;
        const auto actual=load(doc);
        require((actual.materials[0].materialFeatures&(1U<<18U))!=0,
            "Unlit surface feature must survive production glTF loading");
        require(actual.materials[0].emissiveStrength>.29F,
            "Unlit surfaces must preserve explicit emissive energy");
    }
    auto invalid=source;
    auto profile=[&]() -> Json& {return invalid["materials"][0]["extras"]["azureRenderMaterial"];};
    for(const Json& value:{Json(nullptr),Json(false),Json::array(),Json(1)}) {
        invalid=source;profile()["surface"]=value;reject(invalid);
    }
    for(const Json& value:{Json(nullptr),Json(0),Json(1.5),Json(2),Json(true),Json("1")}) {
        invalid=source;profile()["surface"]["schemaVersion"]=value;reject(invalid);
    }
    for(const Json& value:{Json(nullptr),Json(0),Json(true),Json("lit")}) {
        invalid=source;profile()["surface"]["lighting"]=value;reject(invalid);
    }
    for(const char* field:{"schemaVersion","lighting"}) {
        invalid=source;profile()["surface"].erase(field);reject(invalid);
    }
    invalid=source;profile()["surface"]["unknown"]=nullptr;reject(invalid);
    invalid=source;profile()["features"].push_back(nullptr);reject(invalid);
    invalid=source;profile()["features"].push_back("unlit-surface");reject(invalid);
    invalid=source;profile().erase("surface");reject(invalid);
    invalid=source;profile()["features"]={"emissive-mask"};reject(invalid);
    invalid=source;profile()["schemaVersion"]=1;reject(invalid);
    invalid=source;profile()["schemaVersion"]=1.9;reject(invalid);
    for(const char* category:{"face","hair","overlay","fabric"}) {
        invalid=source;profile()["class"]=category;reject(invalid);
    }
    for(const char* feature:{"scene-tint","scene-refraction","hair-anisotropy","silhouette-outline"}) {
        invalid=source;profile()["features"].push_back(feature);reject(invalid);
    }
    invalid=source;
    invalid["materials"][0]["extensions"]["KHR_materials_transmission"]={{"transmissionFactor",.1}};
    reject(invalid);
    invalid=source;
    invalid["materials"][0]["extensions"]["KHR_materials_volume"]={{"thicknessFactor",.1}};
    reject(invalid);
    auto legacy=source;legacy["materials"][0].erase("extras");
    require((load(legacy).materials[0].materialFeatures&(1U<<18U))==0,
        "Legacy surfaces must retain their selected lighting model");
}

int main(int argc,char** argv) { try {
    require(argc==2,"Public material fixture path is required");
    nlohmann::json input;std::ifstream(argv[1])>>input;
    auto& material=input["materials"][0];
    material["pbrMetallicRoughness"]={{"baseColorFactor",{.5,.25,1.,1.}}, {"roughnessFactor",.25},{"metallicFactor",.6}};
    const auto sourceDirectory=std::filesystem::absolute(argv[1]).parent_path();
    auto directory=std::filesystem::temp_directory_path()/("azure-surface-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(directory);
    struct Cleanup{std::filesystem::path path;~Cleanup(){std::error_code error;std::filesystem::remove_all(path,error);}}cleanup{directory};
    unsigned externalFile=0;
    for (const char* section:{"buffers","images"}) for (auto& entry:input[section]) {
        if(entry.contains("uri")) {auto uri=entry["uri"].get<std::string>();
            if(uri.rfind("data:",0)!=0) {
                const auto source=std::filesystem::path(uri).is_relative()?sourceDirectory/uri:std::filesystem::path(uri);
                const auto name="external-"+std::to_string(externalFile++)+source.extension().string();
                std::filesystem::copy_file(source,directory/name);entry["uri"]=name;
            }}
    }
    auto path=directory/"fixture.gltf";
    material["extras"]["azureRenderMaterial"]={{"schemaVersion",2},{"class","hair"},{"features",{"hair-anisotropy"}},
        {"hair",{{"power",200},{"strength",.2},{"rampRow",6},{"strandShift",.07},{"cameraOffset",{0,0,.6}},{"upperLimit",3}}}};
    std::ofstream(path)<<input.dump();
    const auto asset=loadGltfAsset(path.string());const auto& m=asset.materials.at(0);
    require(std::abs(int(m.baseColorPixels[0])-188)<=1,"Linear baseColorFactor must be encoded for sRGB texture storage");
    require(std::abs(int(m.metallicRoughnessPixels[1])-64)<=1,"Untextured roughnessFactor must be preserved");
    require(std::abs(int(m.metallicRoughnessPixels[2])-153)<=1,"Untextured metallicFactor must be preserved");
    require(std::abs(m.hairParameters[2]-6)<.001F,"Hair ramp row must retain its semantic field");
    require(std::abs(m.hairParameters[3]-.07F)<.001F,"Strand shift must be independent from ramp row");
    testCharacterProfileTextures(input,directory);
    testProjectedHairRamp(input,directory);
    testMultiplyComposite(input,directory);
    testUnlitSurface(input,directory);
    material["extras"]["azureRenderMaterial"]["hair"]["rampTexture"]=.5;
    material["extras"]["azureRenderMaterial"]["features"].push_back("hair-ramp");
    std::ofstream(path)<<input.dump();bool invalidRampRejected=false;
    try{(void)loadGltfAsset(path.string());}catch(const std::exception&){invalidRampRejected=true;}
    require(invalidRampRejected,"Fractional hair ramp texture index must be rejected");
    RenderSettings settings;decodeLevelRenderSettings(settings,{{"antiAliasing",2}});
    require(encodeLevelRenderSettings(settings).value("antiAliasing",-1)==2,"Anti-aliasing quality must round trip through production level settings");
    bool rejected=false;try{decodeLevelRenderSettings(settings,{{"antiAliasing",7}});}catch(const std::exception&){rejected=true;}
    require(rejected,"Unknown anti-aliasing quality must be rejected");
    std::cout<<"Surface factors, character texture profiles, linear color and anti-aliasing settings passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
