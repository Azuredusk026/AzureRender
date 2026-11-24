#include "assets/Scene2D.hpp"
#include <iostream>
using namespace azurerender;
int main(){try{
    const auto source=nlohmann::json{{"schemaVersion",1},{"size",{640,360}},{"items",nlohmann::json::array({{{"id","player"},{"kind","sprite"},{"rect",{20,30,40,40}},{"color","#ff0000"},{"layer",1}},{{"id","label"},{"kind","text"},{"rect",{20,10,180,70}},{"text",u8"中文标签 & <test>"},{"layer",2}}})}};
    auto scene=Scene2D::parse(source);const auto rml=scene.toRml();
    if(rml.find("&amp;")==std::string::npos||rml.find("&lt;test&gt;")==std::string::npos||scene.encode()!=source)throw std::runtime_error("2D document must retain identity and escape Chinese label content");
    for(auto invalid:{[&]{auto d=source;d["items"][1]["id"]="player";return d;}(),[&]{auto d=source;d["items"][0]["rect"]={0,0,-1,2};return d;}(),[&]{auto d=source;d["items"][0]["image"]="https://example.invalid/image";return d;}()}){
        bool failed=false;try{Scene2D::parse(invalid);}catch(const std::exception&){failed=true;}if(!failed)throw std::runtime_error("Malformed 2D documents must be rejected");
    }
    std::cout<<"2D scene schema, Chinese labels and safe content passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
