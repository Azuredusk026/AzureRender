// U4 red-test draft. Move into tests only after F5 commit.
#include "foundation/SettingRegistry.hpp"
#include <fstream>
#include <iostream>
#include <thread>
using namespace azurerender;
using Json=nlohmann::json;
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
template<class F>void rejects(F function){bool failed=false;try{function();}catch(const std::exception&){failed=true;}check(failed,"Expected rejection");}
int main(){try{
    SettingRegistry settings;
    settings.add({"quality.samples","Sample count",4,1.,64.,false,true,false});
    settings.add({"editor.scale","Interface scale",1.0,.75,3.,false,true,false});
    settings.add({"device.name","Selected device",std::string("auto"),{},{},true,false,false});
    settings.add({"runtime.workers","Worker count",2,1.,8.,false,true,true});
    for(auto source:{SettingSource::DefaultFile,SettingSource::UserFile,SettingSource::Project,SettingSource::CommandLine,SettingSource::Console}){
        check(settings.set("quality.samples",8,source).passed,"Source write");
        check(settings.get("quality.samples")==4,"Writes wait for frame boundary");
        settings.applyPending();check(settings.get("quality.samples")==8,"Boundary applies candidate");
        check(settings.describe().at("quality.samples").at("source")==settingSourceName(source),"Winning source reported");
        settings.reset("quality.samples",source);settings.applyPending();
        check(settings.get("quality.samples")==4,"Source reset restores default");
    }
    check(settings.replaceLayer({{"quality.samples",5}},SettingSource::DefaultFile).passed,"Default file layer");
    check(settings.replaceLayer({{"quality.samples",7}},SettingSource::Project).passed,"Project layer");
    settings.set("quality.samples",10,SettingSource::Console);settings.applyPending();
    check(settings.get("quality.samples")==10,"All precedence layers retained");
    settings.reset("quality.samples",SettingSource::Console);settings.applyPending();
    check(settings.get("quality.samples")==7,"Reset restores project layer");
    check(settings.replaceLayer(Json::object(),SettingSource::Project).passed,"Empty project layer");settings.applyPending();
    check(settings.get("quality.samples")==5,"Project switch clears project values");
    settings.reset("quality.samples",SettingSource::DefaultFile);settings.applyPending();
    settings.set("quality.samples",12,SettingSource::CommandLine);settings.applyPending();
    settings.set("quality.samples",6,SettingSource::UserFile);settings.applyPending();
    check(settings.get("quality.samples")==12,"Lower source preserves command override");
    check(!settings.set("quality.samples",2.5,SettingSource::Console).passed,"Integer type rejects float");
    check(!settings.set("quality.samples",65,SettingSource::Console).passed,"Range rejection");
    check(!settings.set("device.name","other",SettingSource::Console).passed,"Read-only rejection");
    settings.start();check(!settings.set("runtime.workers",3,SettingSource::Console).passed,"Startup-only rejection");
    const auto folder=std::filesystem::temp_directory_path()/"azure settings source";
    std::filesystem::create_directories(folder);const auto file=folder/"settings.json";
    settings.saveUser(file);
    const auto saved=Json::parse(std::ifstream(file));
    check(saved.at("values").at("quality.samples")==6,"Persistence saves user layer");
    check(!saved.at("values").contains("device.name"),"Read-only field excluded from persistence");
    SettingRegistry reopened;reopened.add({"quality.samples","Sample count",4,1.,64.,false,true,false});
    check(reopened.load(file,SettingSource::UserFile).passed,"User file reload");reopened.applyPending();
    check(reopened.get("quality.samples")==6,"User source restored");
    std::ofstream(file)<<R"({"schemaVersion":1,"values":{"quality.samples":9,"missing":1}})";
    check(!reopened.load(file,SettingSource::UserFile).passed,"Unknown setting fails atomically");
    reopened.applyPending();check(reopened.get("quality.samples")==6,"Partial load preserves values");
    std::ofstream(file)<<R"({"schemaVersion":1.0,"values":{"quality.samples":9}})";
    check(!reopened.load(file,SettingSource::UserFile).passed,"Version requires integer");
    std::ofstream(file)<<R"({"schemaVersion":1,"values":{"quality.samples":9}})";
    check(reopened.load(file,SettingSource::UserFile).passed,"Replacement load succeeds");reopened.applyPending();
    check(reopened.get("quality.samples")==9,"Replacement updates source");
    check(!reopened.replaceLayer({{"quality.samples",10},{"unknown",true}},SettingSource::Console).passed,"Batch validates complete layer");
    reopened.applyPending();check(reopened.get("quality.samples")==9,"Batch failure preserves active value");
    std::ofstream(file)<<"{";
    check(!reopened.load(file,SettingSource::UserFile).passed,"Corrupt settings rejected");
    check(reopened.load(folder/"absent.json",SettingSource::UserFile).passed,"Missing user file uses defaults");
    bool wrongThreadRejected=false;
    std::thread worker([&]{try{settings.get("quality.samples");}catch(const std::exception&){wrongThreadRejected=true;}});worker.join();
    check(wrongThreadRejected,"Registry belongs to owner thread");
    std::filesystem::remove_all(folder);std::cout<<"Setting sources, boundaries and persistence passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
