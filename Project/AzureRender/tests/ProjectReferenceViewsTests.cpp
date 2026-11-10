#include "runtime/ProjectReferenceViews.hpp"
#include "render/ReferenceCameraViewJson.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
#include <limits>
#include <functional>
#include <memory>
#include <cstring>
using namespace azurerender;
using Json=nlohmann::json;
namespace {
void require(bool v,const char* message){if(!v)throw std::runtime_error(message);}
template<class F> void rejects(F f){bool caught=false;try{f();}catch(const std::exception&){caught=true;}require(caught,"Expected configuration rejection");}
std::string bytes(const std::filesystem::path& p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
void write(const std::filesystem::path& p,const std::string& s){std::ofstream f(p,std::ios::binary);f<<s;f.flush();require(bool(f),"Fixture write failed");}
}
int main(int argc,char** argv)try{
    const auto base=std::filesystem::temp_directory_path()/("azure-reference-views-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto moved=base.string()+"-moved";
    auto cleanup=std::unique_ptr<void,std::function<void(void*)>>((void*)1,[&](void*){std::filesystem::remove_all(base);std::filesystem::remove_all(moved);});
    std::filesystem::create_directories(base/"assets");write(base/"assets/startup.azscene","{}");
    write(base/"project.azureproject",R"({"schemaVersion":1,"id":"reference-config-test","name":"Reference views","mounts":[{"name":"assets","path":"assets"}],"startupScene":"assets:/startup.azscene"})");
    auto project=Project::load(base/"project.azureproject");require(loadProjectReferenceViews(project).views.empty(),"Missing configuration must be empty");
    ProjectReferenceViews values;ReferenceCameraView ortho;ortho.extent={640,480};ortho.frame.parameters.axes={-2,3,1,0};ortho.frame.parameters.offset={-0.F,.5F,-1,0};ortho.projection.parameters.columns[0][0]=-0.F;ortho.projection.parameters.columns[0][1]=1;
    ortho.projection.parameters.columns[1][0]=2;ortho.projection.parameters.columns[1][1]=0;ortho.flipNdcY=true;values.views.emplace("inspection.ortho",ortho);
    ReferenceCameraView perspective;perspective.extent={2560,1440};perspective.projection.parameters.columns={ReferenceClipProjectionParameters::Row{2,0,0,0},{0,3,0,0},{0,0,0,-1},{0,0,.1F,0}};perspective.projection.parameters.jitter={1.F/512,-1.F/1024,0,0};values.views.emplace("capture-perspective",perspective);
    if(argc!=1&&argc!=3)throw std::invalid_argument("Expected optional 80-byte projection and 32-byte frame");
    if(argc==3){ReferenceCameraView native;native.extent={2560,1440};std::ifstream p(argv[1],std::ios::binary),f(argv[2],std::ios::binary);
        p.read(reinterpret_cast<char*>(&native.projection.parameters),80);f.read(reinterpret_cast<char*>(&native.frame.parameters),32);require(p.gcount()==80&&f.gcount()==32&&p.peek()==EOF&&f.peek()==EOF,"Invalid native camera input sizes");
        (void)PreparedReferenceCameraView(native);values.views.emplace("native-calibration",native);}
    const auto encoded=encodeProjectReferenceViews(values);const auto decoded=decodeProjectReferenceViews(encoded);require(encodeProjectReferenceViews(decoded)==encoded,"Project codec round trip failed");
    const auto& zero=decoded.views.at("inspection.ortho");require(std::signbit(zero.frame.parameters.offset[0])&&std::signbit(zero.projection.parameters.columns[0][0]),"Signed zero must survive codec");
    unsigned refused=0;auto bad=[&](Json j){rejects([&]{(void)decodeProjectReferenceViews(j);});++refused;};
    Json j=encoded;j["schemaVersion"]=4294967297ULL;bad(j);j=encoded;j["schemaVersion"]=1.0;bad(j);j=encoded;j["extra"]=0;bad(j);j=encoded;j["views"]=Json::array();bad(j);
    j=encoded;j["views"][""]=j["views"].begin().value();bad(j);j=encoded;j["views"]["bad/name"]=j["views"].begin().value();bad(j);
    j=encoded;j["views"][std::string(65,'a')]=j["views"].begin().value();bad(j);
    j=encoded;j["views"]=Json::object();for(unsigned i=0;i<65;++i)j["views"]["view"+std::to_string(i)]=encoded["views"].begin().value();bad(j);
    j=encoded;j["views"]["capture-perspective"]["frame"]["axes"]={1,2,4294967297ULL};bad(j);
    auto view=encoded.at("views").at("capture-perspective");
    for(unsigned which=0;which<13;++which){auto v=view;
        if(which==0)v["schemaVersion"]=2;if(which==1)v["flipNdcY"]=0;if(which==2)v["extent"]={0,1};if(which==3)v["extent"]={8193,1};if(which==4)v["extent"]={1.5,2};
        if(which==5)v["depthConvention"]="invalid";if(which==6)v["frame"]["axes"]={1,1,3};if(which==7)v["frame"]["offset"]={0,0};
        if(which==8)v["projection"]["columns"][0][0]=1.e100;if(which==9)v["projection"]["jitter"]={2,0};if(which==10)v["projection"]["extra"]=0;
        if(which==11)v["projection"]["columns"]=Json::array({Json::array({0,0,0,0}),Json::array({0,0,0,0}),Json::array({0,0,0,0}),Json::array({0,0,0,0})});
        if(which==12)v["frame"]["schemaVersion"]=4294967297ULL;
        j=encoded;j["views"]["capture-perspective"]=v;bad(j);}
    ReferenceCameraView unchanged=perspective;auto invalidJson=view;invalidJson["flipNdcY"]=0;rejects([&]{invalidJson.get_to(unchanged);});require(Json(unchanged)==view,"Failed decode changed caller value");
    saveProjectReferenceViews(project,values);const auto path=projectReferenceViewsPath(project);const auto original=bytes(path);project=Project::load(project.file);
    require(encodeProjectReferenceViews(loadProjectReferenceViews(project))==encoded,"Saved project reopen failed");
    require(std::signbit(loadProjectReferenceViews(project).views.at("inspection.ortho").frame.parameters.offset[0]),"Serialized signed zero lost");
    for(const auto& [name,expected]:values.views){const auto loaded=loadProjectReferenceViews(project).views.at(name);
        require(std::memcmp(&expected.projection.parameters,&loaded.projection.parameters,80)==0&&std::memcmp(&expected.frame.parameters,&loaded.frame.parameters,32)==0,"Persisted camera parameter bits changed");
        require(PreparedReferenceCameraView(expected).clip({.25F,1.5F,-2})==PreparedReferenceCameraView(loaded).clip({.25F,1.5F,-2}),"Reopened camera projection changed");}
    auto invalid=values;invalid.views.begin()->second.projection.parameters.columns[0][0]=std::numeric_limits<float>::infinity();rejects([&]{saveProjectReferenceViews(project,invalid);});require(bytes(path)==original,"Invalid save changed existing file");
    write(path,R"({"schemaVersion":99,"views":{}})");rejects([&]{(void)loadProjectReferenceViews(project);});write(path,original);
    write(path,std::string(1024*1024+1,' '));rejects([&]{(void)loadProjectReferenceViews(project);});write(path,original);
    std::filesystem::rename(base,moved);project=Project::load(std::filesystem::path(moved)/"project.azureproject");require(encodeProjectReferenceViews(loadProjectReferenceViews(project))==encoded,"Moved project reference views failed");
    const auto target=projectReferenceViewsPath(project);std::filesystem::remove(target);std::filesystem::create_directory(target);rejects([&]{saveProjectReferenceViews(project,values);});require(std::filesystem::is_directory(target),"Failed replacement changed target");
    require(std::distance(std::filesystem::directory_iterator(target.parent_path()),std::filesystem::directory_iterator{})==1,"Failed save leaked temporary file");
    std::cout<<values.views.size()<<" reference views, signed-zero persistence, project reopen/move, "<<refused<<" JSON rejections, invalid-save preservation, size boundary and failed-replacement cleanup passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
