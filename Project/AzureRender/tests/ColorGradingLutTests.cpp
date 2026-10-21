#include "render/ColorGradingLut.hpp"
#include "runtime/LevelRenderSettings.hpp"
#include "runtime/AssetDatabase.hpp"
#include "runtime/Level.hpp"
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <cstring>
#include "render/ColorGradingLutGpu.hpp"
#include "rhi/NullRhi.hpp"
#include "runtime/AssetTypeRegistry.hpp"
using namespace azurerender;
void check(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
template<class F> void rejects(F action,const char* message){bool failed=false;try{action();}catch(const std::exception&){failed=true;}check(failed,message);}
void near(float value,float expected,const char* message){check(std::isfinite(value)&&std::abs(value-expected)<2e-6F,message);}
int main(int argc,char** argv) try {
    const auto* lutType=assetTypeRegistry().classify("grade.azlut");
    check(lutType&&lutType->id=="color-grading-lut","Color grading LUT has a registered content type");
    const auto root=std::filesystem::temp_directory_path()/("azure-grade-lut-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(root);
    const auto cleanup=std::unique_ptr<void,std::function<void(void*)>>(reinterpret_cast<void*>(1),[&](void*){std::filesystem::remove_all(root);});
    auto identity=makeIdentityColorGradingLut(4);
    const auto sample=sampleColorGradingLut(identity,{.1F,.7F,.55F});
    near(sample[0],.1F,"R lattice axis");near(sample[1],.7F,"G lattice axis");near(sample[2],.55F,"B slice interpolation");
    const auto edges=sampleColorGradingLut(identity,{-1.F,2.F,1.F});
    near(edges[0],0,"Negative linear inputs clamp to lower boundary");near(edges[1],1,"High linear inputs clamp to upper boundary");near(edges[2],1,"Last slice clamps without cross-row bleed");
    near(sampleColorGradingLut(identity,{.1F,.2F,.3F},2)[0],.2F,"Exposure multiplier is applied before lookup");
    near(sampleColorGradingLut(identity,{.18F,.18F,.18F},1,ColorGradingOutputEncoding::Srgb)[0],.4613561295F,"Explicit output encoding uses sRGB");
    identity.inputDomain=ColorGradingInputDomain::LogC3FullLog;
    near(sampleColorGradingLut(identity,{.18F,0,-1})[0],.39100683F,"LogC3 full logarithmic middle gray");
    near(sampleColorGradingLut(identity,{.18F,0,-1})[1],.06403770373F,"Full logarithmic branch at zero");
    near(sampleColorGradingLut(identity,{.18F,0,-1})[2],0,"Nonpositive logarithm argument maps to lower edge");
    near(colorGradingInputCoordinate(100000.F,ColorGradingInputDomain::LogC3FullLog),1,"HDR log coordinate clamps to top edge");
    rejects([&]{sampleColorGradingLut(identity,{std::numeric_limits<float>::quiet_NaN(),0,0});},"Nonfinite scene input is rejected");
    rejects([&]{sampleColorGradingLut(identity,{1,1,1},0);},"Zero exposure is rejected");
    rejects([&]{makeIdentityColorGradingLut(65);},"Oversized lattice is rejected");
    const auto file=root/"identity.azlut";saveColorGradingLut(file,identity);const auto loaded=loadColorGradingLut(file);
    check(loaded.image.rgba32==identity.image.rgba32&&loaded.inputDomain==identity.inputDomain,"Float32 LUT survives roundtrip");
    const auto mutateHeader=[&](const nlohmann::json& fields){
        saveColorGradingLut(file,identity);std::ifstream input(file,std::ios::binary);
        const std::string bytes((std::istreambuf_iterator<char>(input)),{});input.close();
        const auto* raw=reinterpret_cast<const unsigned char*>(bytes.data()+8);
        const auto size=unsigned(raw[0])|(unsigned(raw[1])<<8)|(unsigned(raw[2])<<16)|(unsigned(raw[3])<<24);
        auto header=nlohmann::json::parse(bytes.substr(12,size));header.merge_patch(fields);const auto text=header.dump();
        std::ofstream output(file,std::ios::binary|std::ios::trunc);output.write("AZLUT001",8);
        for(unsigned shift=0;shift<32;shift+=8)output.put(static_cast<char>((text.size()>>shift)&255));
        output<<text;output.write(bytes.data()+12+size,static_cast<std::streamsize>(bytes.size()-12-size));output.close();
        rejects([&]{loadColorGradingLut(file);},"Invalid LUT schema, domain or shape must be rejected");
    };
    mutateHeader({{"schemaVersion",2}});mutateHeader({{"dimension",2.5}});mutateHeader({{"dimension",4294967300ULL}});
    mutateHeader({{"inputDomain","logc3-piecewise"}});mutateHeader({{"outputDomain","srgb"}});
    mutateHeader({{"source",{{"generatorVersion",-1}}}});mutateHeader({{"source",{{"evidence",3}}}});
    saveColorGradingLut(file,identity);
    auto invalid=identity;invalid.image.rgba32[0]=std::numeric_limits<float>::infinity();
    rejects([&]{saveColorGradingLut(file,invalid);},"Nonfinite LUT values are rejected before save");
    invalid=identity;invalid.image.rgba32[0]=1.01F;rejects([&]{validateColorGradingLut(invalid);},"Out-of-range LUT values are rejected");
    invalid=identity;invalid.source.generator.clear();rejects([&]{validateColorGradingLut(invalid);},"Missing provenance is rejected");
    invalid=identity;invalid.source.generatorVersion=0;rejects([&]{validateColorGradingLut(invalid);},"Invalid provenance version is rejected");
    invalid=identity;invalid.image.width--;rejects([&]{validateColorGradingLut(invalid);},"Wrong strip shape is rejected");
    rhi::NullRhi backend;backend.collectImageUploads=true;
    {ColorGradingLutGpu resource(backend,identity);
        check(resource.view()&&resource.sampler()&&resource.dimension()==4,"Public GPU resource owns view and sampler");
        check(backend.imageUploads.size()==1&&backend.imageUploads[0].pixels.size()==identity.image.rgba32.size()*sizeof(float),"GPU upload preserves float32 samples");
        check(backend.allocator().statistics().liveImages==1&&backend.allocator().statistics().liveBuffers==0,"GPU resource releases staging allocation after upload");
    }
    check(backend.allocator().statistics().liveImages==0,"GPU resource releases owned image");
    backend.unavailableSampledFormats.insert(VK_FORMAT_R32G32B32A32_SFLOAT);
    rejects([&]{ColorGradingLutGpu unavailable(backend,identity);},"Unsupported sampling format reports device limitation");
    check(backend.allocator().statistics().liveImages==0,"Failed GPU load retains no image allocation");
    {ColorGradingLutGpu placeholder(backend,makeIdentityColorGradingLut(),ColorGradingLutUpload::IdentityPlaceholder);
        check(placeholder.view()!=VK_NULL_HANDLE,"Default pipeline requires no optional float32 filtering capability");
    }
    check(backend.allocator().statistics().liveImages==0,"Default placeholder releases its image");
    {std::ofstream append(file,std::ios::binary|std::ios::app);append.put('x');}
    rejects([&]{loadColorGradingLut(file);},"Trailing pixel bytes are rejected");
    RenderSettings settings;decodeLevelRenderSettings(settings,{{"gradeLut",{{"schemaVersion",1},{"asset","assets:/grade.azlut"},{"exposureMultiplier",2.5F}}}});
    const auto persisted=encodeLevelRenderSettings(settings);
    check(persisted.at("gradeLut").at("asset")=="assets:/grade.azlut","Portable LUT reference is serialized");
    for(const auto& bad:{nlohmann::json{{"schemaVersion",2},{"asset","x"},{"exposureMultiplier",1}},
        nlohmann::json{{"schemaVersion",1},{"asset","x"},{"exposureMultiplier",-1}},
        nlohmann::json{{"schemaVersion",1},{"asset","x"},{"exposureMultiplier",1},{"unknown",1}}}){
        rejects([&]{decodeLevelRenderSettings(settings,{{"gradeLut",bad}});},"Invalid settings are rejected");
        check(encodeLevelRenderSettings(settings)==persisted,"Invalid settings edits preserve the authoring state");
    }
    auto document=SceneDocument::fromAsset("engine:/assets_public/test_model.gltf");document.renderSettings=settings;
    document.save(root/"scene.azscene");const auto reopened=SceneDocument::load(root/"scene.azscene");
    check(encodeLevelRenderSettings(reopened.renderSettings).at("gradeLut")==persisted.at("gradeLut"),"Scene save and reopen retain LUT settings");
    saveColorGradingLut(file,identity);
    auto standalone=document;standalone.renderSettings.grade.lut.asset="identity.azlut";
    standalone.save(root/"standalone.azscene");
    const auto reopenedStandalone=SceneDocument::load(root/"standalone.azscene");
    check(reopenedStandalone.renderSettings.grade.lut.asset=="identity.azlut"
        &&reopenedStandalone.renderSettings.grade.lut.resolvedAsset==std::filesystem::weakly_canonical(file),
        "Standalone scene resolves a neighboring LUT from a different working directory");
    check(migrateRenderSettings(settings,13).grade.lut.asset.empty(),"Older settings migrate to the default grade pipeline");
    Project::create(root/"project","Grade fixture");auto project=Project::load(root/"project/project.azureproject");
    const auto assetFile=root/"project/assets/grade.azlut";saveColorGradingLut(assetFile,identity);
    AssetDatabase assets(project);assets.refresh();const auto assetId=assets.idForPath("assets:/grade.azlut");
    nlohmann::json level={{"schemaVersion",1},{"id","grade-level"},{"resources",nlohmann::json::array()},{"nodes",nlohmann::json::array()},
        {"renderSettings",{{"gradeLut",{{"schemaVersion",1},{"asset",assetId},{"exposureMultiplier",1}}}}}};
    {std::ofstream out(root/"project/assets/grade.azurelevel");out<<level.dump();}
    assets.refresh();const auto levelId=assets.idForPath("assets:/grade.azurelevel");
    check(assets.records().at(levelId).dependencies==std::vector<std::string>{assetId},"Level declares LUT resource dependency");
    const auto first=Level::parse(level,assets);
    check(first.scene.renderSettings.grade.lut.asset==assetId&&!first.scene.renderSettings.grade.lut.resolvedAsset.empty(),"Runtime resolves LUT without overwriting portable reference");
    SceneDocument authored;authored.sceneId="grade-scene";authored.renderSettings=first.scene.renderSettings;
    authored.save(root/"project/assets/grade.azscene");assets.refresh();
    const auto sceneId=assets.idForPath("assets:/grade.azscene");
    check(assets.records().at(sceneId).dependencies==std::vector<std::string>{assetId},"Standalone scene declares LUT dependency");
    const auto projectScene=project.loadScene(root/"project/assets/grade.azscene");
    check(projectScene.renderSettings.grade.lut.asset==assetId&&projectScene.renderSettings.grade.lut.resolvedAsset==assetFile,"Project scene resolves portable LUT reference");
    const auto key=first.resourceKey;
    identity.image.rgba32[0]=.2F;saveColorGradingLut(assetFile,identity);assets.refresh(true);
    check(Level::parse(level,assets).resourceKey!=key,"LUT changes invalidate the runtime resource key");
    std::filesystem::rename(assetFile,root/"project/assets/moved.azlut");
    std::filesystem::rename(assetFile.string()+".azmeta",root/"project/assets/moved.azlut.azmeta");assets.refresh();
    check(Level::parse(level,assets).scene.renderSettings.grade.lut.resolvedAsset.filename()=="moved.azlut","UUID survives resource move");
    std::filesystem::rename(root/"project",root/"relocated");
    auto movedProject=Project::load(root/"relocated/project.azureproject");AssetDatabase movedAssets(movedProject);movedAssets.refresh();
    check(Level::parse(level,movedAssets).scene.renderSettings.grade.lut.asset==assetId,"Project relocation retains LUT reference");
    if(argc==2){
        const auto candidate=loadColorGradingLut(std::filesystem::u8path(argv[1]));
        check(candidate.inputDomain==ColorGradingInputDomain::LogC3FullLog&&candidate.source.evidence==ColorGradingEvidence::Candidate,
            "Native generated candidate conforms to the production loader contract");
        const auto gray=sampleColorGradingLut(candidate,{.18F,.18F,.18F});
        std::cout<<"Native candidate loaded: N="<<candidate.dimension<<" gray="<<gray[0]<<','<<gray[1]<<','<<gray[2]<<'\n';
    }
    std::cout<<"Color grading LUT resource, interpolation, domains and portable persistence passed\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
