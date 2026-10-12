#include "editor/console/ConsoleLogModel.hpp"
#include <iostream>
#include <stdexcept>
using namespace azurerender;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
int main(){try{
    const auto info="[10][info][assets][code=0] imported error-report.txt";
    const auto warning="[11][warning][shader][code=0] 文件路径警告";
    const auto error="[12][error][runtime][code=5] failed\nsecond line";
    require(consoleMessageLevel(warning)==DiagnosticLevel::Warning,"Warning header must produce warning severity");
    require(consoleMessageLevel(error)==DiagnosticLevel::Error,"Error header must produce error severity");
    require(consoleMessageLevel(info)==DiagnosticLevel::Info,"Error words in informational messages must not change severity");
    require(consoleMessageLevel("Unformatted error output")==DiagnosticLevel::Info,"CLI output remains informational");
    require(consoleMessageLevel("[not-a-time][error] text")==DiagnosticLevel::Info,"Malformed headers must not impersonate diagnostic severity");
    ConsoleLogModel model;model.update({info,warning,error});
    const auto all=[](const std::string&){return true;};
    auto rows=model.visible(DiagnosticLevel::Warning,all);
    require(rows==std::vector<std::size_t>({1,2}),"Warning filter includes warning and error rows only");
    require(model.copyVisible(rows)==std::string(warning)+"\n"+error+"\n","Copy must match filtered rows and preserve multiline text");
    rows=model.visible(DiagnosticLevel::Info,[](const auto& text){return text.find("文件")!=std::string::npos;});
    require(rows==std::vector<std::size_t>({1}),"Unicode searches must retain matching identity");
    model.setPaused(true);model.update({"new message"});
    require(model.entries().size()==3&&model.entries()[0].text==info,"Paused log view must retain its snapshot");
    model.setPaused(false);model.update({"new message"});
    require(model.entries().size()==1&&model.entries()[0].text=="new message","Resume must replace the snapshot with current diagnostics");
    model.update({});require(model.visible(DiagnosticLevel::Info,all).empty()&&model.copyVisible({}).empty(),"Empty snapshots produce an empty view and clipboard");
    std::cout<<"Console severity, filtering, pause and copy passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
