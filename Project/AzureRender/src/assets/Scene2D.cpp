#include "assets/Scene2D.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <set>
#include <sstream>
namespace azurerender {
namespace {
std::string escape(const std::string& value){std::string out;for(char c:value){switch(c){case '&':out+="&amp;";break;case '<':out+="&lt;";break;case '>':out+="&gt;";break;case '\"':out+="&quot;";break;case '\'':out+="&#39;";break;default:out+=c;}}return out;}
bool id(const std::string& value){return !value.empty()&&value.size()<=64&&value.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-")==std::string::npos;}
void number(const nlohmann::json& value,double minimum,double maximum){if(!value.is_number()||!std::isfinite(value.get<double>())||value.get<double>()<minimum||value.get<double>()>maximum)throw std::invalid_argument("Invalid 2D coordinate or size");}
}
Scene2D Scene2D::parse(const nlohmann::json& document){
    if(!document.is_object()||document.size()!=3||document.at("schemaVersion")!=1||!document.at("size").is_array()||document.at("size").size()!=2||!document.at("items").is_array()||document.at("items").size()>10000)throw std::invalid_argument("Invalid 2D scene schema");
    for(const auto& dimension:document.at("size"))number(dimension,1,8192);
    std::set<std::string> ids;
    for(const auto& item:document.at("items")){
        const auto name=item.at("id").get<std::string>(),kind=item.at("kind").get<std::string>();
        if(!id(name)||!ids.insert(name).second||(kind!="sprite"&&kind!="text"&&kind!="rectangle")||!item.at("rect").is_array()||item.at("rect").size()!=4)throw std::invalid_argument("Invalid 2D item");
        for(std::size_t i=0;i<4;++i)number(item.at("rect")[i],i<2?-100000:1,100000);
        if(item.contains("layer"))number(item.at("layer"),-10000,10000);
        if(item.contains("fontSize"))number(item.at("fontSize"),1,512);
        if(item.contains("color")){const auto color=item.at("color").get<std::string>();if((color.size()!=7&&color.size()!=9)||color[0]!='#'||color.substr(1).find_first_not_of("0123456789abcdefABCDEF")!=std::string::npos)throw std::invalid_argument("2D color must be hexadecimal");}
        if(kind=="text"&&item.at("text").get<std::string>().size()>65536)throw std::invalid_argument("2D text budget exceeded");
        if(item.contains("image")){const auto image=item.at("image").get<std::string>();if(image.empty()||image.size()>1024||image.find("..")!=std::string::npos||image.find_first_of(":\\\"'<>\n\r")!=std::string::npos||image.front()=='/')throw std::invalid_argument("2D images require local relative paths");}
        for(const auto& field:item.items())if(field.key()!="id"&&field.key()!="kind"&&field.key()!="rect"&&field.key()!="color"&&field.key()!="layer"&&field.key()!="fontSize"&&field.key()!="image"&&field.key()!="text")throw std::invalid_argument("Unknown 2D item field");
    }
    Scene2D scene;scene.document_=document;return scene;
}
Scene2D Scene2D::load(const std::filesystem::path& path){if(std::filesystem::file_size(path)>8*1024*1024)throw std::invalid_argument("2D document budget exceeded");std::ifstream stream(path);if(!stream)throw std::runtime_error("2D document missing");nlohmann::json document;stream>>document;return parse(document);}
std::string Scene2D::toRml()const{
    std::ostringstream out;out<<"<rml><head><style>body{font-family:AzureGame;font-size:20px;margin:0;} .item{position:absolute;}</style></head><body>";
    auto items=document_.at("items").get<std::vector<nlohmann::json>>();std::stable_sort(items.begin(),items.end(),[](const auto& a,const auto& b){return a.value("layer",0)<b.value("layer",0);});
    for(const auto& item:items){const auto& rect=item.at("rect");const bool text=item.at("kind")=="text";
        out<<"<div class=\"item\" id=\""<<item.at("id").get<std::string>()<<"\" style=\"left:"<<rect[0].get<double>()<<"px;top:"<<rect[1].get<double>()<<"px;width:"<<rect[2].get<double>()<<"px;height:"<<rect[3].get<double>()<<"px;";
        out<<(text?"color:":"background-color:")<<item.value("color",text?"#ffffff":"#ffffffff")<<";font-size:"<<item.value("fontSize",20)<<"px;\">";
        if(text)out<<escape(item.at("text").get<std::string>());
        else if(item.contains("image"))out<<"<img src=\""<<escape(item.at("image").get<std::string>())<<"\" style=\"width:100%;height:100%;\"/>";
        out<<"</div>";
    }out<<"</body></rml>";return out.str();
}
}
