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
