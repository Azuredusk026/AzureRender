#include "ui/localization/Localization.hpp"
#include <string>
#include <iostream>
using namespace azurerender::ui;
int main(){
    setLanguage(Language::Chinese);if(std::string(tr("Save"))!="保存")return 1;
    if(std::string(tr("mipFilter"))!="多级纹理过滤"||std::string(tr("Mip bias"))!="多级纹理偏移")return 6;
    setLanguage(Language::English);if(std::string(tr("场景###viewport"))!="Scene###viewport")return 2;
    if(std::string(tr("项目：%s"))!="Project: %s")return 3;
    if(std::string(tr("user-defined.asset"))!="user-defined.asset")return 4;
    const char* retained=tr("保存");setLanguage(Language::Chinese);
    if(std::string(retained)!="Save"||std::string(tr("场景###viewport"))!="场景###viewport")return 5;
    std::cout<<"Language switching, stable panel IDs, formatting and user text passed\n";
}
