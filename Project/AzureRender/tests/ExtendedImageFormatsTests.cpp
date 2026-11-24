#include "assets/image/ImageCodec.hpp"
#include "assets/AssetTransfer.hpp"
#include <chrono>
#include <fstream>
#include <iostream>
using namespace azurerender;
int main(){const auto root=std::filesystem::temp_directory_path()/("azure-extra-images-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));try{
    std::filesystem::create_directories(root);auto check=[](bool value,const char* message){if(!value)throw std::runtime_error(message);};
    auto write=[&](const std::string& name,const std::vector<unsigned char>& bytes){std::ofstream stream(root/name,std::ios::binary);stream.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));};
    const std::vector<unsigned char> gif={'G','I','F','8','9','a',1,0,1,0,0x80,0,0,255,0,0,0,0,0,0x2c,0,0,0,0,1,0,1,0,0,2,2,0x44,1,0,0x3b};write("red.gif",gif);
    std::vector<unsigned char> psd={'8','B','P','S',0,1,0,0,0,0,0,0,0,3,0,0,0,1,0,0,0,1,0,8,0,3};psd.insert(psd.end(),14,0);psd.insert(psd.end(),{255,0,0});write("red.psd",psd);
    const std::string svg="<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"4\" height=\"4\"><rect width=\"4\" height=\"4\" fill=\"#ff0000\"/></svg>";write("red.svg",{svg.begin(),svg.end()});
    for(const auto* format:{"gif","psd","svg"}){const auto source=root/("red."+std::string(format));check(assetFormatRegistry().detect(source).id==format,"Extension and content must detect extended image formats");const auto image=decodeImageFile(source);const auto pixels=convertImageToRgba8(image);check(pixels.pixels[0]==255&&pixels.pixels[1]==0&&pixels.pixels[3]==255,"Extended image must decode actual red pixels");if(std::string(format)!="svg")check(!pixels.losses.empty(),"Static imports must disclose animation or layer losses");}
    for(const auto& bad:std::vector<std::string>{"<svg><image href=\"https://example.invalid/a.png\"/></svg>","<svg><script>bad</script></svg>","<svg width=\"99999\" height=\"1\"/>","<svg><path d=\"broken\"/></svg>"}){
        bool failed=false;try{decodeImageBytes({bad.begin(),bad.end()});}catch(const std::exception&){failed=true;}check(failed,"Unsafe, oversized or malformed SVG must fail");}
    bool cancelled=false;try{decodeImageBytes(gif,[]{return true;});}catch(const std::exception&){cancelled=true;}check(cancelled,"Extended image decoding must honor cancellation");
    std::filesystem::remove_all(root);std::cout<<"GIF, PSD, SVG pixels, losses, rejection and cancellation passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';std::filesystem::remove_all(root);return 1;}}
