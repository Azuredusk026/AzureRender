#include "assets/GltfLoader.hpp"
#include "assets/MaterialData.hpp"
#include "assets/MaterialSurfaceProfile.hpp"
#include <cstring>
#include "runtime/LevelRenderSettings.hpp"
#include <nlohmann/json.hpp>
#include <stb_image_write.h>
#include <fstream>
#include <iostream>
#include <cmath>
#include <chrono>
using namespace azurerender;
void require(bool value, const char* message) { if (!value) throw std::runtime_error(message); }

void testOutlineSurface(const nlohmann::json& fixture,const std::filesystem::path& directory) {
    using Json=nlohmann::json;
    auto source=fixture;
    source["materials"][0]["extras"]={{"azureRenderMaterial",{
        {"schemaVersion",2},{"class","generic"},{"features",{"silhouette-outline"}},
        {"outlineSurface",{{"schemaVersion",1},{"mode","view-scaled"},{"width",.5},{"depthPrepass",true}}}}}};
    const auto path=directory/"outline-surface.gltf";
    const auto load=[&](const Json& value) {std::ofstream(path)<<value.dump();return loadGltfAsset(path.string());};
    const auto profile=[](Json& value)->Json& {return value["materials"][0]["extras"]["azureRenderMaterial"]["outlineSurface"];};
    const auto reject=[&](const Json& value,const char* field) {
        std::string failure;try{(void)load(value);}catch(const std::exception& error){failure=error.what();}
        if(failure.empty()||failure.find(field)==std::string::npos)
            throw std::runtime_error(std::string("Invalid outline input must identify ")+field+"; actual: "+failure);
    };
    const auto asset=load(source);
    require(!asset.vertices.empty(),"An outline profile must retain its production geometry");
    const auto& outline=asset.materials.at(0).outlineSurface;
    require(outline.present&&outline.depthPrepass&&outline.width==.5F&&outline.minimumPixels==.5F
        &&outline.normalSource==AssetMaterial::OutlineSurface::NormalSource::Geometry,"Outline fields must reach the typed material");
    require(outline.maskPixels==std::vector<std::uint8_t>{255,0,0,255}&&outline.maskWidth==1
        &&outline.maskHeight==1,"Absent masks must retain width and zero depth defaults");
    for(const char* field:{"width","minimumPixels","depthOffset"})for(const auto& value:Json::array({nullptr,true,"0",-9,9})) {
        auto invalid=source;profile(invalid)[field]=value;reject(invalid,field);
    }
    for(const char* field:{"schemaVersion","mode","normalSource","depthPrepass","uvScaleOffset","color"}) {
        auto invalid=source;profile(invalid)[field]=nullptr;reject(invalid,field);
    }
    auto invalid=source;profile(invalid)["unexpected"]=1;reject(invalid,"unexpected");
    auto smooth=source;profile(smooth)["normalSource"]="tangent-uv1";
    for(auto& mesh:smooth["meshes"])for(auto& primitive:mesh["primitives"])primitive["attributes"].erase("TEXCOORD_1");
    reject(smooth,"TEXCOORD_1");
    for(auto& mesh:smooth["meshes"])for(auto& primitive:mesh["primitives"])
        primitive["attributes"]["TEXCOORD_1"]=primitive["attributes"]["TEXCOORD_0"];
    auto withStream=load(smooth);
    require(withStream.texcoord1.size()==withStream.vertices.size()&&withStream.primitives.at(0).hasTexcoord1,
        "Optional UV1 must keep a parallel stream and explicit primitive participation");
    require(withStream.texcoord1.front()==withStream.vertices.front().texcoord,"UV1 must preserve authored coordinates");
    auto octahedral=smooth;profile(octahedral)["normalSource"]="tangent-octahedral-uv1";
    const auto fullSphere=load(octahedral);
    require(static_cast<unsigned>(fullSphere.materials.at(0).outlineSurface.normalSource)==2
        &&fullSphere.texcoord1==withStream.texcoord1,"Octahedral smooth directions must retain their explicit stream encoding");
    auto mixed=smooth;profile(mixed)["normalSource"]="geometry";
    auto withoutStream=mixed["meshes"][0]["primitives"][0];withoutStream["attributes"].erase("TEXCOORD_1");
    mixed["meshes"][0]["primitives"].push_back(withoutStream);
    const auto mixedAsset=load(mixed);
    require(mixedAsset.texcoord1.size()==mixedAsset.vertices.size()&&mixedAsset.primitives.at(0).hasTexcoord1
        &&!mixedAsset.primitives.back().hasTexcoord1,"Mixed primitives must keep independent UV1 participation");
    require(mixedAsset.texcoord1.at(withStream.vertices.size())==std::array<float,2>{0,0},
        "Absent UV1 sections must keep their aligned zero storage");
    withStream.texcoord1.at(withStream.indices.at(0))={1.25F,0};
    bool rejected=false;try{validateOutlineMaterialInputs(withStream);}catch(const std::invalid_argument&){rejected=true;}
    require(rejected,"Material binding must reject out-of-range smooth coordinates");
    const std::vector<std::uint8_t> pixels{128,64,0,255};
    require(stbi_write_png((directory/"outline-mask.png").string().c_str(),1,1,4,pixels.data(),4)!=0,"Cannot write outline mask fixture");
    auto textured=source;const auto image=textured["images"].size(),sampler=textured["samplers"].size(),texture=textured["textures"].size();
    textured["images"].push_back({{"uri","outline-mask.png"}});
    textured["samplers"].push_back({{"wrapS",33071},{"wrapT",33071},{"magFilter",9729},{"minFilter",9729}});
    textured["textures"].push_back({{"source",image},{"sampler",sampler}});profile(textured)["maskTexture"]=texture;
    require(load(textured).materials.at(0).outlineSurface.maskPixels==pixels,"Vertex masks must preserve linear channels");
    invalid=textured;profile(invalid)["maskTexture"]=double(texture)+.00000001;reject(invalid,"maskTexture");
    invalid=textured;invalid["samplers"][sampler]["minFilter"]=9987;reject(invalid,"sampler");
    const auto metadata=directory/"outline-mask.png.azmeta";
    std::ofstream(metadata)<<Json{{"settings",{{"texture",{{"colorSpace","srgb"}}}}}}.dump();
    reject(textured,"settings");std::filesystem::remove(metadata);
    const std::array<std::uint32_t,32> ddsHeader{
        0x20534444,124,0x2100f,2,2,8,0,2,
        0,0,0,0,0,0,0,0,0,0,0,
        32,0x41,0,32,0xff,0xff00,0xff0000,0xff000000,
        0x401008,0,0,0,0};
    const std::vector<std::uint8_t> ddsBase{128,64,0,255,17,83,1,254,96,32,2,253,29,149,3,252};
    {std::ofstream output(directory/"outline-mask.dds",std::ios::binary);
        output.write(reinterpret_cast<const char*>(ddsHeader.data()),sizeof(ddsHeader));
        output.write(reinterpret_cast<const char*>(ddsBase.data()),ddsBase.size());
        const std::array<unsigned char,4> lower{11,22,33,44};
        output.write(reinterpret_cast<const char*>(lower.data()),lower.size());}
    auto mipMask=textured;mipMask["images"][image]["uri"]="outline-mask.dds";
    const auto mipMaterial=load(mipMask).materials.at(0).outlineSurface;
    require(mipMaterial.maskWidth==2&&mipMaterial.maskHeight==2&&mipMaterial.maskPixels==ddsBase,
        "Fixed-LOD outline masks must retain exactly the base image when DDS contains authored mips");
}

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
    auto unrelatedAo=source;
    unrelatedAo["materials"][0]["occlusionTexture"]={{"index",hair}};
    std::ofstream(directory/"hair-data.png.azmeta")<<Json{{"settings",{{"texture",{{"filter","nearest"}}}}}};
    require(load(unrelatedAo).materials[0].textureSettings[4].filter=="linear",
        "An unrelated glTF AO image must not override the selected diffuse-ramp sampler");
    std::filesystem::remove(directory/"hair-data.png.azmeta");
    std::ofstream(directory/"diffuse-ramp.png.azmeta")<<Json{{"settings",{{"texture",{{"filter","nearest"}}}}}};
    reject(source,"fixed lookup sampler","Diffuse-ramp metadata must respect the fixed lookup contract");
    std::ofstream(directory/"diffuse-ramp.png.azmeta")<<Json{{"settings",{{"texture",{{"mipBias",-1}}}}}};
    reject(source,"fixed lookup sampler","Lookup textures must reject a mip-bias override");
    std::filesystem::remove(directory/"diffuse-ramp.png.azmeta");
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
    auto handed=source;handed["materials"][0]["extras"]["azureRenderMaterial"]["hair"]["projectedRamp"]["tangentSign"]=-1;
    require(packMaterialSurfaceProfile(load(handed).materials[0]).rows[4][3]==-1,
        "Explicit projected tangent orientation must reach its reserved GPU component");
    for(const auto& value:Json::array({0,.5,2,true,"-1",nullptr})) {
        auto invalid=handed;invalid["materials"][0]["extras"]["azureRenderMaterial"]["hair"]["projectedRamp"]["tangentSign"]=value;
        reject(invalid,"projectedRamp.tangentSign");
    }
    auto virtualSource=source;
    virtualSource["materials"][0]["extras"]["azureRenderMaterial"]["hair"]["projectedRamp"]["virtualLight"]={
        {"schemaVersion",1},{"blend",1.0},{"objectGain",.75}};
    const auto virtualAsset=load(virtualSource);
    require(virtualAsset.materials[0].projectedHairRamp.virtualLight&&virtualAsset.materials[0].projectedHairRamp.objectGain==.75F,
        "Projected hair must preserve explicit virtual-light parameters");
    require(packMaterialSurfaceProfile(virtualAsset.materials[0]).rows[6]==std::array<float,4>{1,1,.75F,0},"Virtual-light parameters must reach the GPU layout");
    auto striped=source;
    striped["materials"][0]["extras"]["azureRenderMaterial"]["hair"]["projectedRamp"]["line"]={
        {"schemaVersion",1},{"texture",texture},{"frequency",300},{"strandShift",.4},{"range",.95},{"depth",.22},
        {"alternation",1},{"saturation",1.5},{"uvTransform",{9.3,1,-.32,0}}};
    const auto stripedAsset=load(striped);const auto& line=stripedAsset.materials[0].projectedHairRamp.line;
    require(line.present&&line.frequency==300&&line.pixels.size()>0,"Independent specular-line texture must reach its typed owner");
    require(packMaterialSurfaceProfile(stripedAsset.materials[0]).rows[8]==std::array<float,4>{1,1.5F,1,0},"Line parameters must reach their dedicated GPU rows");
    auto combinedLine=striped;
    combinedLine["materials"][0]["extras"]["azureRenderMaterial"]["hair"]["projectedRamp"]["line"]["diffuseTarget"]="main-and-ambient";
    require(packMaterialSurfaceProfile(load(combinedLine).materials[0]).rows[8][3]==1,
        "Explicit main and ambient line modulation must reach its reserved GPU component");
    for(const auto& value:Json::array({"all","",1,true,nullptr})) {
        auto invalid=combinedLine;
        invalid["materials"][0]["extras"]["azureRenderMaterial"]["hair"]["projectedRamp"]["line"]["diffuseTarget"]=value;
        reject(invalid,"diffuseTarget");
    }
    // The actual sampled image owns metadata, including typed hair inputs.
    auto sampled=striped;
    std::filesystem::copy_file(directory/"projected-ramp.png",directory/"sampled-hair.png",
        std::filesystem::copy_options::overwrite_existing);
    const int sampledImage=static_cast<int>(sampled["images"].size());
    sampled["images"].push_back({{"uri","sampled-hair.png"}});
    const int sampledTexture=static_cast<int>(sampled["textures"].size());
    sampled["textures"].push_back({{"source",sampledImage}});
    auto& sampledHair=sampled["materials"][0]["extras"]["azureRenderMaterial"]["hair"];
    sampledHair["dataTexture"]=sampledTexture;
    sampledHair["projectedRamp"]["parameterTexture"]=sampledTexture;
    sampledHair["projectedRamp"]["line"]["texture"]=sampledTexture;
    const auto sampledSidecar=directory/"sampled-hair.png.azmeta";
    std::ofstream(sampledSidecar)<<Json{{"settings",{{"texture",{{"anisotropy",16},{"wrapU","mirror"},{"mipBias",-1},{"mipFilter","nearest"}}}}}};
    const auto sampledMaterial=load(sampled).materials[0];
    for(unsigned slot:{6U,7U,8U})
        require(sampledMaterial.textureSettings[slot].anisotropy==16
            &&sampledMaterial.textureSettings[slot].wrapU=="mirror"
            &&sampledMaterial.textureSettings[slot].mipBias==-1
            &&!sampledMaterial.textureSettings[slot].sampler(8,16).mipmapLinear
            &&sampledMaterial.textureSettings[slot].colorSpace=="linear",
            "Typed hair samplers must inherit metadata from their selected source images");
    Json complete={{"schemaVersion",3},{"typeId","azure.profile"},{"parameters",Json::object()},
        {"textures",Json::object()},{"surface",{{"asset",{{"version","2.0"}}},
            {"materials",Json::array({sampled["materials"][0]})},{"textures",sampled["textures"]}}}};
    std::map<std::string,std::filesystem::path> imagePaths;
    for(std::size_t index=0;index<sampled["images"].size();++index) {
        auto uuid=std::string("00000000-0000-4000-8000-")+std::string(12-std::to_string(index).size(),'0')+std::to_string(index);
        complete["textures"][std::to_string(index)]={{"asset",uuid},{"usage","linear"}};
        imagePaths[uuid]=directory/std::filesystem::u8path(sampled["images"][index]["uri"].get<std::string>());
    }
    const auto resolved=MaterialData::parse(complete).interpret([&](const auto& uuid){return imagePaths.at(uuid);});
    for(unsigned slot:{6U,7U,8U})
        require(resolved.textureSettings[slot].anisotropy==16&&resolved.textureSettings[slot].wrapU=="mirror"&&resolved.textureSettings[slot].mipBias==-1
            &&!resolved.textureSettings[slot].sampler(8,16).mipmapLinear,
            "Complete UUID materials must preserve typed image sampling metadata");
    std::ofstream(sampledSidecar)<<Json{{"settings",{{"texture",{{"usage","albedo"},{"colorSpace","srgb"}}}}}};
    reject(sampled,"typed texture color space");
    bool rejectedUuid=false;
    try{(void)MaterialData::parse(complete).interpret([&](const auto& uuid){return imagePaths.at(uuid);});}
    catch(const std::exception& error){rejectedUuid=std::string(error.what()).find("typed texture color space")!=std::string::npos;}
    require(rejectedUuid,"Complete UUID materials must reject incompatible typed image metadata");
    std::filesystem::remove(sampledSidecar);
    for(const auto& value:Json::array({-1,17,"1",true,nullptr})) {
        auto invalid=virtualSource;invalid["materials"][0]["extras"]["azureRenderMaterial"]["hair"]["projectedRamp"]["virtualLight"]["objectGain"]=value;reject(invalid);
    }
    for(const char* field:{"schemaVersion","texture","frequency","strandShift","range","depth","alternation","saturation","uvTransform"}) {
        auto invalid=striped;invalid["materials"][0]["extras"]["azureRenderMaterial"]["hair"]["projectedRamp"]["line"].erase(field);reject(invalid);
    }
    for(const auto& value:Json::array({-1,1.1,"1",true,nullptr})) {
        auto invalid=striped;invalid["materials"][0]["extras"]["azureRenderMaterial"]["hair"]["projectedRamp"]["line"]["depth"]=value;reject(invalid);
    }
    auto lineConflict=striped;lineConflict["materials"][0]["extras"]["azureRenderMaterial"]["shadowColorLut"]={{"schemaVersion",1},{"texture",texture}};reject(lineConflict);
    auto framed=source;const auto frameNode=framed["nodes"].size();
    framed["nodes"].push_back({{"name","independent-head"}});
    framed["materials"][0]["extras"]["azureRenderMaterial"]["hair"]["projectedRamp"]["frame"]={
        {"schemaVersion",1},{"node","independent-head"},{"basis",{1,0,0,0,1,0,0,0,1}}};
    const auto framedAsset=load(framed);const auto& actualFrame=framedAsset.materials[0].projectedHairRamp.frame;
    require(actualFrame.present&&actualFrame.node==frameNode&&actualFrame.nodeName=="independent-head",
        "Independent projected hair must resolve its explicit frame without a face SDF owner");
    for(const auto& badFrame:Json::array({nullptr,{{"schemaVersion",1},{"node","missing"},{"basis",{1,0,0,0,1,0,0,0,1}}},
        {{"schemaVersion",1},{"node","independent-head"},{"basis",{1,0,0,1,0,0,0,0,1}}}})){
        auto invalid=framed;invalid["materials"][0]["extras"]["azureRenderMaterial"]["hair"]["projectedRamp"]["frame"]=badFrame;
        reject(invalid,"frame");
    }
    auto duplicateFrame=framed;duplicateFrame["nodes"].push_back({{"name","independent-head"}});reject(duplicateFrame,"frame");
    auto conflictingFrame=framed;conflictingFrame["materials"][1]=conflictingFrame["materials"][0];
    conflictingFrame["materials"][1]["extras"]["azureRenderMaterial"]["hair"]["projectedRamp"]["frame"]["basis"]={0,1,0,-1,0,0,0,0,1};
    reject(conflictingFrame,"frames");
    auto strand=source;
    auto& strandProfile=strand["materials"][0]["extras"]["azureRenderMaterial"]["hair"]["projectedRamp"];
    strandProfile["surfaceResponse"]="strand-diffuse";strandProfile["diffuseScale"]=.96;
    const auto strandAsset=load(strand);
    require(strandAsset.materials[0].projectedHairRamp.surfaceResponse==AssetMaterial::ProjectedHairRamp::SurfaceResponse::StrandDiffuse
        &&std::abs(strandAsset.materials[0].projectedHairRamp.diffuseScale-.96F)<.000001F,
        "Strand surface response and independent diffuse energy must survive loading");
    require(hair.surfaceResponse==AssetMaterial::ProjectedHairRamp::SurfaceResponse::Layered&&hair.diffuseScale==1,
        "Projected ramp defaults must retain the layered surface contract");
    auto litStrand=strand;
    require(stbi_write_png((directory/"lighting-ramp.png").string().c_str(),2,1,4,pixels.data(),8)!=0,
        "Cannot write diffuse lighting ramp fixture");
    litStrand["images"].push_back({{"uri","lighting-ramp.png"}});
    litStrand["textures"].push_back({{"source",litStrand["images"].size()-1}});
    auto& litProfile=litStrand["materials"][0]["extras"]["azureRenderMaterial"];
    litProfile["features"].push_back("diffuse-ramp");
    litProfile["diffuseRamp"]={{"schemaVersion",1},{"texture",litStrand["textures"].size()-1}};
    litProfile["hair"]["projectedRamp"]["lighting"]={{"schemaVersion",1},{"shadowBrightness",.55},
        {"shadowSaturation",1.1},{"shadowScale",.4},{"ambientColor",{.8,.9,1.1}},{"mainColor",{1,.9,.7}}};
    const auto litAsset=load(litStrand);const auto litPacked=packMaterialSurfaceProfile(litAsset.materials[0]);
    require(litPacked.rows[5][3]==.4F&&litPacked.rows[6][3]==1
        &&litPacked.rows[10]==std::array<float,4>{.8F,.9F,1.1F,.55F}
        &&litPacked.rows[11]==std::array<float,4>{1,.9F,.7F,1.1F},
        "Projected hair lighting must preserve typed colors and independent shadow controls");
    for(const auto& field:{"schemaVersion","shadowBrightness","shadowSaturation","shadowScale","ambientColor","mainColor"}) {
        auto bad=litStrand;bad["materials"][0]["extras"]["azureRenderMaterial"]["hair"]["projectedRamp"]["lighting"].erase(field);
        reject(bad,"projectedRamp.lighting");
    }
    for(const auto* field:{"shadowScale","shadowBrightness","shadowSaturation"})
        for(const auto& badValue:Json::array({true,nullptr,"1",-1,17})) {
            auto bad=litStrand;bad["materials"][0]["extras"]["azureRenderMaterial"]["hair"]["projectedRamp"]["lighting"][field]=badValue;
            reject(bad,"projectedRamp.lighting");
        }
    for(const auto& badValue:Json::array({true,nullptr,"1",1.0,0,2})) {
        auto bad=litStrand;bad["materials"][0]["extras"]["azureRenderMaterial"]["hair"]["projectedRamp"]["lighting"]["schemaVersion"]=badValue;
        reject(bad,"projectedRamp.lighting");
    }
    for(const auto& badValue:Json::array({true,nullptr,1,Json::array({1,1}),Json::array({1,1,-1}),Json::array({1,1,65})})) {
        auto bad=litStrand;bad["materials"][0]["extras"]["azureRenderMaterial"]["hair"]["projectedRamp"]["lighting"]["ambientColor"]=badValue;
        reject(bad,"projectedRamp.lighting");
    }
    auto badLighting=litStrand;badLighting["materials"][0]["extras"]["azureRenderMaterial"]["hair"]["projectedRamp"]["surfaceResponse"]="layered";
    reject(badLighting,"projectedRamp.lighting");
    badLighting=litStrand;badLighting["materials"][0]["extras"]["azureRenderMaterial"].erase("diffuseRamp");
    badLighting["materials"][0]["extras"]["azureRenderMaterial"]["features"]={"hair-anisotropy","hair-projected-ramp"};
    reject(badLighting,"projectedRamp.lighting");
    badLighting=litStrand;badLighting["materials"][0]["extras"]["azureRenderMaterial"]["hair"]["projectedRamp"]["lighting"]["unknown"]=1;
    reject(badLighting,"projectedRamp.lighting");
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

// Include before CharacterRepairTests.cpp main; call testEyeRadialSurface(input,directory).
// The numeric feature bit permits a tests-only build against existing headers.
void testMaterialSurfacePacking() {
    AssetMaterial::ProjectedHairRamp hair;
    hair.primaryShift=.1F;hair.secondaryShift=.2F;hair.intensity=.3F;hair.secondaryRange=.4F;
    hair.edgeFade=.5F;hair.specularNormalScale=.6F;hair.directionX=.7F;hair.surfaceNormalScale=.8F;
    hair.secondaryColor={.9F,1,1.1F};hair.objectBasis={1,2,3,4,5,6,7,8,9};
    hair.surfaceResponse=AssetMaterial::ProjectedHairRamp::SurfaceResponse::StrandDiffuse;hair.diffuseScale=.96F;
    const std::array<float,24> expectedHair{.1F,.2F,.3F,.4F,.5F,.6F,.7F,.8F,.9F,1,1.1F,1,1,2,3,.96F,4,5,6,1,7,8,9,0};
    const auto packedHair=packProjectedHairSurfaceProfile(hair);
    for(std::size_t row=6;row<packedHair.rows.size();++row)require(packedHair.rows[row]==std::array<float,4>{},"Inactive extension rows must remain zero");
    require(std::memcmp(&packedHair,expectedHair.data(),sizeof(expectedHair))==0,
        "Typed hair packing must preserve every declared component of the six-row prefix");
    AssetMaterial material;material.materialClass=AssetMaterialClass::Eye;auto& eye=material.eyeSurface;eye.present=true;
    eye.parallaxScale=.15F;eye.matcapNormalScale=1.5F;eye.alphaPremultiply=.25F;eye.matcapShadowFloor=.75F;
    eye.highlightColor={2,3,4};eye.scatteringColor={5,6,7};eye.matcapColor={8,9,10,11};
    eye.directWeights={12,13,14};eye.uvScaleOffset={15,16,17,18};
    const std::array<float,24> expectedEye{.15F,1.5F,.25F,.75F,2,3,4,0,5,6,7,0,8,9,10,11,12,13,14,0,15,16,17,18};
    const auto packedEye=packEyeSurfaceProfile(eye);
    require(std::memcmp(&packedEye,expectedEye.data(),sizeof(expectedEye))==0,
        "Typed eye packing must preserve all authored and reserved components");
    const auto selected=packMaterialSurfaceProfile(material);
    require(std::memcmp(&selected,expectedEye.data(),sizeof(expectedEye))==0,"Eye selects the typed eye record");
    material.eyeSurface.present=false;material.materialClass=AssetMaterialClass::Hair;material.projectedHairRamp=hair;
    const auto generic=packMaterialSurfaceProfile(material);
    require(std::memcmp(&generic,expectedHair.data(),sizeof(expectedHair))==0,"Other materials retain stable hair-compatible rows");
}

void testEyeRadialSurface(const nlohmann::json& fixture,const std::filesystem::path& directory) {
    using Json=nlohmann::json;
    constexpr std::uint32_t eyeBit=1U<<17U;
    const std::vector<std::uint8_t> basePixels{128,64,224,128};
    const std::vector<std::uint8_t> rampPixels{17,64,128,223, 96,32,224,160};
    const std::vector<std::uint8_t> matcapPixels{32,96,160,224, 96,32,224,160,
                                               17,83,149,215, 255,0,1,254};
    require(stbi_write_png((directory/"eye-base.png").string().c_str(),1,1,4,basePixels.data(),4)!=0,
        "Cannot write eye base fixture");
    require(stbi_write_png((directory/"eye-ramp.png").string().c_str(),2,1,4,rampPixels.data(),8)!=0,
        "Cannot write eye ramp fixture");
    require(stbi_write_png((directory/"eye-matcap.png").string().c_str(),2,2,4,matcapPixels.data(),8)!=0,
        "Cannot write eye Matcap fixture");
    // Embedded geometry matches gpu-candidates/direct-linear-control/assets/eye.gltf.
    auto source=Json::parse(R"eye({
      "asset":{"version":"2.0"},"scene":0,"scenes":[{"nodes":[0]}],
      "nodes":[{"mesh":0}],"meshes":[{"primitives":[{"attributes":{
        "POSITION":0,"NORMAL":1,"TANGENT":2,"TEXCOORD_0":3},"indices":4,"material":0}]}],
      "buffers":[{"byteLength":204,"uri":"data:application/octet-stream;base64,ZmZmv2ZmZr8AAAAAZmZmP2ZmZr8AAAAAZmZmP2ZmZj8AAAAAZmZmv2ZmZj8AAAAAAAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/AAAAAAAAAAAAAIA/AACAPwAAAAAAAAAAAACAPwAAgD8AAAAAAAAAAAAAgD8AAIA/AAAAAAAAAAAAAIA/AACAPwAAAAAAAAAAAACAP1K4Hj9SuB4/UrgeP1K4Hj9SuB4/UrgeP1K4Hj9SuB4/AAABAAIAAAACAAMA"}],
      "bufferViews":[{"buffer":0,"byteOffset":0,"byteLength":48},
        {"buffer":0,"byteOffset":48,"byteLength":48},{"buffer":0,"byteOffset":96,"byteLength":64},
        {"buffer":0,"byteOffset":160,"byteLength":32},{"buffer":0,"byteOffset":192,"byteLength":12}],
      "accessors":[{"bufferView":0,"componentType":5126,"count":4,"type":"VEC3",
        "min":[-0.9,-0.9,0],"max":[0.9,0.9,0]},
        {"bufferView":1,"componentType":5126,"count":4,"type":"VEC3"},
        {"bufferView":2,"componentType":5126,"count":4,"type":"VEC4"},
        {"bufferView":3,"componentType":5126,"count":4,"type":"VEC2"},
        {"bufferView":4,"componentType":5123,"count":6,"type":"SCALAR"}]
    })eye");
    source["images"]={{{"uri","eye-base.png"}},{{"uri","eye-ramp.png"}},{{"uri","eye-matcap.png"}}};
    source["textures"]={{{"source",0},{"sampler",0}},{{"source",1},{"sampler",0}},{{"source",2},{"sampler",0}}};
    source["samplers"]={{{"wrapS",33071},{"wrapT",33071},{"magFilter",9729},{"minFilter",9729}}};
    Json authoredMaterial={{"name","radial-eye-fixture"},{"doubleSided",true},{"alphaMode","OPAQUE"}};
    authoredMaterial["pbrMetallicRoughness"]={{"metallicFactor",0},{"roughnessFactor",1}};
    authoredMaterial["pbrMetallicRoughness"]["baseColorTexture"]={{"index",0}};
    Json authoredProfile={{"schemaVersion",2},{"class","eye"}};
    authoredProfile["features"]=Json::array({"eye-radial-surface"});
    Json authoredEye={
        {"schemaVersion",1},{"diffuseRampTexture",1},{"matcapTexture",2},
        {"parallaxScale",.15},{"matcapNormalScale",1},{"alphaPremultiply",.5},
        {"highlightColor",{3.4415908,3.0091395,2.8109329}},
        {"scatteringColor",{0,3.2831938,16.94838}},
        {"matcapColor",{1.2754714,.23824833,.73298275,2}},
        {"uvScaleOffset",{1,1,-.25,.5}}};
    Json authoredLighting={{"source","host-authored"},{"illuminationModel","host-lambert"},
        {"matcapShadowFloor",.3},{"directWeights",{1,.25,.5}}};
    authoredEye["lightingInputs"]=authoredLighting;
    authoredProfile["eyeSurface"]=authoredEye;
    authoredMaterial["extras"]["azureRenderMaterial"]=authoredProfile;
    source["materials"]=Json::array({authoredMaterial});
    const auto path=directory/"eye-radial-surface.gltf";
    const auto load=[&](const Json& document) {
        std::ofstream(path)<<document.dump();
        return loadGltfAsset(path.string());
    };
    const auto loadBinary=[&](const Json& document) {
        auto raw=document.dump();while(raw.size()%4)raw.push_back(' ');
        std::vector<unsigned char> bytes;
        const auto word=[&](std::uint32_t value) {
            for(unsigned byte=0;byte<4;++byte)bytes.push_back(static_cast<unsigned char>(value>>(byte*8U)));
        };
        word(0x46546c67U);word(2);word(static_cast<std::uint32_t>(20+raw.size()));
        word(static_cast<std::uint32_t>(raw.size()));word(0x4e4f534aU);
        bytes.insert(bytes.end(),raw.begin(),raw.end());
        const auto binaryPath=directory/"eye-radial-surface.glb";
        std::ofstream stream(binaryPath,std::ios::binary);
        stream.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));stream.close();
        return loadGltfAsset(binaryPath.string());
    };
    const auto profile=[](Json& document) -> Json& {return document["materials"][0]["extras"]["azureRenderMaterial"];};
    const auto eye=[&](Json& document) -> Json& {return profile(document)["eyeSurface"];};
    const auto lighting=[&](Json& document) -> Json& {return eye(document)["lightingInputs"];};
    const auto reject=[&](const Json& document,const char* property="",bool binary=false) {
        std::string failure;
        try{(void)(binary?loadBinary(document):load(document));}catch(const std::exception& error){failure=error.what();}
        if(failure.empty()||(*property&&failure.find(property)==std::string::npos))
            throw std::runtime_error(std::string("Eye invalid input requires rejection for ")+property
                +"; actual diagnostic: "+failure);
    };

    // This first production load supplies a real semantic red before implementation.
    const auto asset=load(source);
    const auto& actual=asset.materials.at(0);
    const auto sidecar=directory/"eye-base.png.azmeta";
    std::ofstream(sidecar)<<Json{{"settings",{{"texture",{{"colorSpace","linear"}}}}}}.dump();
    reject(source,"eyeSurface");
    std::filesystem::remove(sidecar);
    (void)load(source);
    require(actual.materialClass==AssetMaterialClass::Eye&&(actual.materialFeatures&eyeBit)!=0,
        "Explicit radial eye feature and class must survive production loading");
    require(actual.alphaMode==AssetAlphaMode::Opaque&&actual.materialProfileVersion==2,
        "Eye surfaces require opaque parent material schema 2");
    require(actual.baseColorWidth==1&&actual.baseColorHeight==1&&actual.baseColorPixels==basePixels,
        "Eye base must preserve raw sRGB RGB and independent linear alpha bytes");
    require(actual.styleMaskWidth==2&&actual.styleMaskHeight==1&&actual.styleMaskPixels==rampPixels,
        "Eye ramp must preserve both linear RGBA samples and horizontal dimensions");
    require(actual.matcapWidth==2&&actual.matcapHeight==2&&actual.matcapPixels==matcapPixels,
        "Eye Matcap must preserve all sRGB RGB and linear alpha source bytes");
    require(actual.textureSettings[0].colorSpace=="srgb"&&actual.textureSettings[4].colorSpace=="linear"
        &&actual.textureSettings[5].colorSpace=="srgb",
        "Eye slots 0/4/5 require their independent color-space semantics");
    for(const unsigned slot:{4U,5U}) {
        const auto& settings=actual.textureSettings[slot];
        require(settings.wrapU=="clamp"&&settings.wrapV=="clamp"&&settings.mipPolicy=="none"
            &&settings.filter=="linear"&&settings.anisotropy==1,
            "Eye ramp and Matcap require clamp linear sampling without mips or anisotropy");
    }
    const auto& imported=actual.eyeSurface;
    require(imported.present&&std::abs(imported.parallaxScale-.15F)<1e-6F
        &&imported.matcapNormalScale==1&&imported.alphaPremultiply==.5F
        &&std::abs(imported.matcapShadowFloor-.3F)<1e-6F,"Typed eye scalar fields retain their explicit values");
    const auto compareVector=[&](const auto& values,const Json& expected,const char* reason) {
        require(values.size()==expected.size(),reason);
        for(std::size_t index=0;index<values.size();++index)
            require(std::abs(values[index]-expected[index].template get<float>())<1e-5F,reason);
    };
    for(const auto& field:std::array<std::pair<const char*,std::array<float,3>>,3>{{
        {"highlightColor",imported.highlightColor},{"scatteringColor",imported.scatteringColor},
        {"directWeights",imported.directWeights}}}) {
        const auto& expected=std::string(field.first)=="directWeights"?lighting(source)[field.first]:eye(source)[field.first];
        compareVector(field.second,expected,"Typed eye HDR colors and direct weights must remain unclamped");
    }
    compareVector(imported.matcapColor,eye(source)["matcapColor"],"Typed eye Matcap RGBA must remain unclamped");
    compareVector(imported.uvScaleOffset,eye(source)["uvScaleOffset"],"Typed eye UV transform preserves signed components");
    auto outline=source;profile(outline)["features"].push_back("silhouette-outline");
    require((load(outline).materials.at(0).materialFeatures&MaterialFeatureSilhouetteOutline)!=0,
        "Eye surfaces permit an independent silhouette outline");
    auto omittedAlpha=source;omittedAlpha["materials"][0].erase("alphaMode");(void)load(omittedAlpha);
    auto nprEye=source;
    lighting(nprEye)["illuminationModel"]="npr-main-ambient";
    lighting(nprEye)["ambientColor"]={1.2,.9,.8};
    lighting(nprEye)["mainColor"]={1,.92,.86};
    lighting(nprEye)["shadowScale"]=.55;
    const auto nprMaterial=load(nprEye).materials.at(0);
    require(nprMaterial.eyeSurface.nprLighting&&std::abs(nprMaterial.eyeSurface.shadowScale-.55F)<1e-6F,
        "Eye NPR lighting fields must reach the typed material");
    const auto nprPacked=packMaterialSurfaceProfile(nprMaterial);
    require(nprPacked.rows[6]==std::array<float,4>{.55F,0,0,1}
        &&nprPacked.rows[7]==std::array<float,4>{1.2F,.9F,.8F,0}
        &&nprPacked.rows[8]==std::array<float,4>{1,.92F,.86F,0},"Eye NPR lighting must retain its explicit shared record fields");
    for(const char* field:{"ambientColor","mainColor","shadowScale"}) {
        auto invalid=nprEye;lighting(invalid).erase(field);reject(invalid,field);
        for(const auto& value:Json::array({nullptr,true,"0",-1,65})) {
            invalid=nprEye;lighting(invalid)[field]=value;reject(invalid,field);
        }
    }
    auto nprMip=nprEye;nprMip["samplers"].push_back({{"wrapS",33071},{"wrapT",33071},{"magFilter",9729},{"minFilter",9987}});
    nprMip["textures"][2]["sampler"]=1;
    require(load(nprMip).materials.at(0).textureSettings[5].mipPolicy=="generate","Eye NPR Matcap must retain its authored mip sampler");
    nprMip["samplers"][1]["minFilter"]=9985;
    require(load(nprMip).materials.at(0).textureSettings[5].mipFilter==TextureImportSettings::MipFilter::Nearest,
        "Eye NPR Matcap must distinguish linear filtering with nearest mip selection");
    auto caster=nprEye;profile(caster)["features"].push_back("geometry-shadow");
    require((load(caster).materials.at(0).materialFeatures&MaterialFeatureGeometryShadow)!=0,"Eye geometry shadow ownership must survive the eye profile");
    (void)loadBinary(source);(void)loadBinary(omittedAlpha);
    auto alternate=fixture;
    for(const char* section:{"images","textures","samplers"})alternate[section]=source[section];
    alternate["materials"][0]=source["materials"][0];
    require((load(alternate).materials.at(0).materialFeatures&eyeBit)!=0,
        "Eye material loading must also work with an independent mesh fixture");
    auto legacy=source;profile(legacy).erase("eyeSurface");profile(legacy)["features"]=Json::array();
    require((load(legacy).materials.at(0).materialFeatures&eyeBit)==0,
        "Legacy eye materials retain their explicit feature selection");

    for(const auto& value:Json::array({nullptr,true,"2",0,1,1.9,2.1,3,1e300})) {
        auto invalid=source;profile(invalid)["schemaVersion"]=value;reject(invalid,"schemaVersion");
    }
    for(const auto& value:Json::array({nullptr,true,"1",0,1.5,2,1e300})) {
        auto invalid=source;eye(invalid)["schemaVersion"]=value;reject(invalid,"eyeSurface");
    }
    for(const char* field:{"schemaVersion","diffuseRampTexture","matcapTexture","parallaxScale",
        "matcapNormalScale","alphaPremultiply","highlightColor","scatteringColor","matcapColor",
        "uvScaleOffset","lightingInputs"}) {
        auto invalid=source;eye(invalid).erase(field);reject(invalid,"eyeSurface");
        invalid=source;eye(invalid)[field]=nullptr;reject(invalid,"eyeSurface");
    }
    for(const auto& value:Json::array({nullptr,true,0,"surface",Json::array()})) {
        auto invalid=source;profile(invalid)["eyeSurface"]=value;reject(invalid,"eyeSurface");
    }
    auto invalid=source;profile(invalid).erase("eyeSurface");reject(invalid,"eyeSurface");
    invalid=source;profile(invalid).erase("schemaVersion");reject(invalid,"schemaVersion");
    invalid=source;profile(invalid).erase("class");reject(invalid);
    invalid=source;profile(invalid)["class"]=nullptr;reject(invalid);
    invalid=source;profile(invalid).erase("features");reject(invalid);
    for(const auto& value:Json::array({nullptr,true,0,"eye-radial-surface",Json::array()})) {
        invalid=source;profile(invalid)["features"]=value;reject(invalid);
    }
    invalid=source;profile(invalid)["features"].push_back("eye-radial-surface");reject(invalid,"features");
    invalid=outline;profile(invalid)["features"].push_back("silhouette-outline");reject(invalid,"features");
    invalid=source;profile(invalid)["features"].push_back(nullptr);reject(invalid,"features");
    for(const char* category:{"generic","skin","face","hair","fabric","metal","overlay","emissive","showcase"}) {
        invalid=source;profile(invalid)["class"]=category;reject(invalid);
    }
    for(const auto& alpha:Json::array({"MASK","BLEND",nullptr,true,0,"opaque",Json::object(),Json::array()})) {
        invalid=source;invalid["materials"][0]["alphaMode"]=alpha;
        reject(invalid,"eyeSurface");reject(invalid,"eyeSurface",true);
    }
    for(const auto& value:Json::array({nullptr,1})) {
        invalid=source;eye(invalid)["unknown"]=value;reject(invalid,"eyeSurface");
        invalid=source;lighting(invalid)["unknown"]=value;reject(invalid,"lightingInputs");
    }
    for(const char* field:{"source","illuminationModel","matcapShadowFloor","directWeights"}) {
        invalid=source;lighting(invalid).erase(field);reject(invalid,"lightingInputs");
        invalid=source;lighting(invalid)[field]=nullptr;reject(invalid,"lightingInputs");
    }
    for(const auto& value:Json::array({"native-capture","unknown",true,1,Json::array()})) {
        invalid=source;lighting(invalid)["source"]=value;reject(invalid,"lightingInputs");
    }
    for(const auto& value:Json::array({"native-npr","unknown",true,1,Json::array()})) {
        invalid=source;lighting(invalid)["illuminationModel"]=value;reject(invalid,"lightingInputs");
    }
    for(const char* field:{"parallaxScale","matcapNormalScale","alphaPremultiply"}) {
        for(const auto& value:Json::array({true,"0",-.01,1e300,Json::array()})) {
            invalid=source;eye(invalid)[field]=value;reject(invalid,field);
        }
        invalid=source;eye(invalid)[field]=std::string(field)=="matcapNormalScale"?8.01:1.01;reject(invalid,field);
    }
    for(const auto& value:Json::array({true,"0",-.01,1.01,1e300,Json::array()})) {
        invalid=source;lighting(invalid)["matcapShadowFloor"]=value;reject(invalid,"matcapShadowFloor");
    }
    for(const char* field:{"highlightColor","scatteringColor","matcapColor","uvScaleOffset"}) {
        const unsigned count=(std::string(field)=="matcapColor"||std::string(field)=="uvScaleOffset")?4:3;
        for(const auto& value:Json::array({true,1,"color",Json::array(),Json::array({0,0})})) {
            invalid=source;eye(invalid)[field]=value;reject(invalid,field);
        }
        invalid=source;eye(invalid)[field]=Json::array();
        for(unsigned index=0;index<=count;++index)eye(invalid)[field].push_back(0);
        reject(invalid,field);
        for(const auto& value:Json::array({nullptr,true,"0",1e300})) {
            invalid=source;eye(invalid)[field][0]=value;reject(invalid,field);
        }
        if(std::string(field)!="uvScaleOffset")for(const double value:{-.01,64.01}) {
            invalid=source;eye(invalid)[field][0]=value;reject(invalid,field);
        }
    }
    for(const auto& value:Json::array({true,1,"weights",Json::array(),Json::array({0,0}),Json::array({0,0,0,0})})) {
        invalid=source;lighting(invalid)["directWeights"]=value;reject(invalid,"directWeights");
    }
    for(const auto& value:Json::array({nullptr,true,"0",-.01,64.01,1e300})) {
        invalid=source;lighting(invalid)["directWeights"][0]=value;reject(invalid,"directWeights");
    }
    for(const char* field:{"diffuseRampTexture","matcapTexture"}) {
        for(const auto& value:Json::array({true,"1",-1,.5,99999,1e300})) {
            invalid=source;eye(invalid)[field]=value;reject(invalid,field);
        }
        const int index=std::string(field)=="diffuseRampTexture"?1:2;
        for(const int image:{-1,99999}) {
            invalid=source;invalid["textures"][index]["source"]=image;reject(invalid);
        }
        invalid=source;invalid["textures"][index].erase("source");reject(invalid);
    }
    for(const int image:{0,2}) {
        invalid=source;invalid["textures"][1]["source"]=image;reject(invalid);
    }
    for(const char* feature:{"stylized-shadow","hair-anisotropy","face-sdf-eligible","emissive-mask",
        "overlay","neutral-fallback","brow-overlay","surface-ao","scene-tint","hair-ramp",
        "scene-refraction","dual-face-sdf","shadow-color-lut","diffuse-ramp","multiply-composite",
        "hair-projected-ramp","unlit-surface"}) {
        invalid=source;profile(invalid)["features"].push_back(feature);reject(invalid);
    }
    for(const char* field:{"lamShadowColor","matcapColor","hair","faceSdf","diffuseRamp","refraction","multiplyComposite"}) {
        // Null fields must retain their authored ownership even if TinyGLTF omits them.
        invalid=source;profile(invalid)[field]=nullptr;reject(invalid);
        invalid=source;profile(invalid)[field]=Json::object();reject(invalid);
    }
    invalid=source;invalid["materials"][0]["extensions"]["KHR_materials_transmission"]={{"transmissionFactor",.1}};reject(invalid);
    invalid=source;invalid["materials"][0]["extensions"]["KHR_materials_volume"]={{"thicknessFactor",.1}};reject(invalid);
    for(const char* extension:{"KHR_materials_unlit","KHR_materials_ior","KHR_materials_emissive_strength","unknown"}) {
        invalid=source;invalid["materials"][0]["extensions"][extension]=Json::object();reject(invalid,"eyeSurface");
    }
    invalid=source;invalid["textures"][0]["source"]=-1;reject(invalid,"baseColorTexture");
    invalid=source;invalid["textures"][0]["extensions"]["unknown"]=Json::object();reject(invalid,"baseColorTexture");

    // Inclusive endpoints and finite signed UV transforms are legal host inputs.
    for(const double endpoint:{0.,1.}) {
        auto legal=source;eye(legal)["parallaxScale"]=endpoint;eye(legal)["alphaPremultiply"]=endpoint;
        eye(legal)["matcapNormalScale"]=endpoint*8;lighting(legal)["matcapShadowFloor"]=endpoint;
        lighting(legal)["directWeights"]={endpoint*64,endpoint*64,endpoint*64};
        eye(legal)["highlightColor"]={endpoint*64,endpoint*64,endpoint*64};
        eye(legal)["scatteringColor"]={endpoint*64,endpoint*64,endpoint*64};
        eye(legal)["matcapColor"]={endpoint*64,endpoint*64,endpoint*64,endpoint*64};
        eye(legal)["uvScaleOffset"]={-2,0,-3,4};(void)load(legal);
    }
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
    auto standalone=input;
    for(const char* key:{"buffers","bufferViews","accessors","meshes","scenes","scene","animations","skins"})standalone.erase(key);
    // Preserve named nodes used by SDF profiles; geometry is not required.
    for(auto& node:standalone["nodes"]){node.erase("mesh");node.erase("skin");}
    for(auto& image:standalone["images"])if(image.contains("uri")&&image["uri"].get<std::string>().rfind("data:",0)!=0){
        std::ifstream stream(directory/image["uri"].get<std::string>(),std::ios::binary);
        require(bool(stream),"Standalone material fixture image must be readable");
        const std::vector<unsigned char> data{std::istreambuf_iterator<char>(stream),{}};
        const char* alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string encoded;unsigned accumulator=0;int bits=-6;
        for(unsigned char byte:data){accumulator=(accumulator<<8)|byte;bits+=8;while(bits>=0){encoded+=alphabet[(accumulator>>bits)&63];bits-=6;}}
        if(bits>-6)encoded+=alphabet[((accumulator<<8)>>(bits+8))&63];
        while(encoded.size()%4)encoded+='=';
        image["uri"]="data:image/png;base64,"+encoded;
    }
    const auto standaloneMaterials=loadEmbeddedGltfMaterials(standalone.dump());
    auto externalMaterial=standalone;externalMaterial["images"][0]["uri"]="external-image.png";
    bool externalRejected=false;try{(void)loadEmbeddedGltfMaterials(externalMaterial.dump());}catch(const std::exception&){externalRejected=true;}
    require(externalRejected,"Standalone embedded materials must reject external resource access");
    require(standaloneMaterials.size()==input.at("materials").size(),"Material-only import must preserve the authored table");
    const auto& independent=standaloneMaterials.at(0);
    require(independent.materialClass==m.materialClass&&independent.materialFeatures==m.materialFeatures
        &&independent.hairParameters==m.hairParameters&&independent.baseColorPixels==m.baseColorPixels
        &&independent.metallicRoughnessPixels==m.metallicRoughnessPixels,
        "Standalone and model materials must share physical, character and texture interpretation");
    for(const auto& version:std::vector<nlohmann::json>{1.5,2.5,true,nullptr}){
        auto invalid=input;invalid["materials"][0]["extras"]["azureRenderMaterial"]["schemaVersion"]=version;
        std::ofstream(path)<<invalid.dump();bool rejected=false;
        try{(void)loadGltfAsset(path.string());}catch(const std::exception&){rejected=true;}
        require(rejected,"Material profile versions require an exact supported integer");
    }
    for(const auto& vector:std::vector<nlohmann::json>{nullptr,{1,2,3},{1,"bad",3,4}}){
        auto invalid=input;invalid["materials"][0]["extras"]["azureRenderMaterial"]["styleParameters"]=vector;
        std::ofstream(path)<<invalid.dump();bool rejected=false;
        try{(void)loadGltfAsset(path.string());}catch(const std::exception&){rejected=true;}
        require(rejected,"Authored material vectors must reject malformed values");
    }
    auto duplicateFeatures=input;duplicateFeatures["materials"][0]["extras"]["azureRenderMaterial"]["features"]={"hair-anisotropy","hair-anisotropy"};
    std::ofstream(path)<<duplicateFeatures.dump();bool duplicateRejected=false;
    try{(void)loadGltfAsset(path.string());}catch(const std::exception&){duplicateRejected=true;}
    require(duplicateRejected,"Duplicate material features must be rejected");
    testCharacterProfileTextures(input,directory);
    testProjectedHairRamp(input,directory);
    testOutlineSurface(input,directory);
    testEyeRadialSurface(input,directory);
    testMaterialSurfacePacking();
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
