#include "assets/image/SvgCodec.hpp"
#include <pugixml.hpp>
#include <cmath>
#include <memory>
#include <set>
#ifdef _MSC_VER
#pragma warning(push,0)
#endif
#define NANOSVG_IMPLEMENTATION
#include "../../../third_party/nanosvg/nanosvg.h"
#define NANOSVGRAST_IMPLEMENTATION
#include "../../../third_party/nanosvg/nanosvgrast.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif
namespace azurerender {
bool isSvgImage(const std::vector<unsigned char>& bytes){
    const std::string text(bytes.begin(),bytes.begin()+std::min<std::size_t>(bytes.size(),65536));
    std::size_t pos=text.size()>=3&&static_cast<unsigned char>(text[0])==0xef&&static_cast<unsigned char>(text[1])==0xbb&&static_cast<unsigned char>(text[2])==0xbf?3:0;
    for(;;){pos=text.find_first_not_of(" \r\n\t",pos);if(pos==std::string::npos)return false;
        if(text.compare(pos,5,"<?xml")==0){pos=text.find("?>",pos);if(pos==std::string::npos)return false;pos+=2;continue;}
        if(text.compare(pos,4,"<!--")==0){pos=text.find("-->",pos);if(pos==std::string::npos)return false;pos+=3;continue;}
        return text.compare(pos,4,"<svg")==0&&pos+4<text.size()&&(text[pos+4]=='>'||text[pos+4]=='/'||text[pos+4]==' '||text[pos+4]=='\n'||text[pos+4]=='\r'||text[pos+4]=='\t');
    }
}
DecodedImage decodeSvgImage(const std::vector<unsigned char>& bytes,const AssetCancellation& cancelled){
    if(bytes.size()>8*1024*1024)throw std::invalid_argument("SVG source exceeds 8 MiB");
    std::string source(bytes.begin(),bytes.end());
    if(source.find("<!DOCTYPE")!=std::string::npos||source.find("<!ENTITY")!=std::string::npos)throw std::invalid_argument("SVG document declarations are unsupported");
    pugi::xml_document document;
    if(!document.load_buffer(bytes.data(),bytes.size())||std::string(document.document_element().name())!="svg")throw std::invalid_argument("Malformed SVG XML");
    const std::set<std::string> tags={"svg","g","defs","linearGradient","radialGradient","stop","path","rect","circle","ellipse","polygon","polyline","line","title","desc"};
    std::vector<pugi::xml_node> pending{document.document_element()};std::size_t count=0;
    while(!pending.empty()){
        if(cancelled&&cancelled())throw std::runtime_error("SVG decode cancelled");
        const auto node=pending.back();pending.pop_back();if(++count>10000)throw std::invalid_argument("SVG node budget exceeded");
        if(!tags.count(node.name()))throw std::invalid_argument("Unsupported SVG element: "+std::string(node.name()));
        for(const auto& attribute:node.attributes()){
            const std::string name=attribute.name(),value=attribute.value();
            if(name.rfind("on",0)==0||name=="href"||name=="xlink:href")throw std::invalid_argument("SVG references and event handlers are unsupported");
            for(std::size_t pos=value.find("url(");pos!=std::string::npos;pos=value.find("url(",pos+4))if(value.compare(pos,5,"url(#")!=0)throw std::invalid_argument("SVG external paint references are unsupported");
            if(name=="d"&&(value.empty()||value.find_first_not_of(" \t\r\n") == std::string::npos||std::string("Mm").find(value[value.find_first_not_of(" \t\r\n")])==std::string::npos))throw std::invalid_argument("SVG paths require an initial move command");
        }
        for(auto child:node.children())if(child.type()==pugi::node_element)pending.push_back(child);
    }
    source.push_back('\0');std::unique_ptr<NSVGimage,decltype(&nsvgDelete)> image(nsvgParse(source.data(),"px",96),nsvgDelete);
    if(!image||!std::isfinite(image->width)||!std::isfinite(image->height)||image->width<1||image->height<1||image->width>8192||image->height>8192)throw std::invalid_argument("Invalid SVG dimensions");
    for(auto* shape=image->shapes;shape;shape=shape->next)for(auto* path=shape->paths;path;path=path->next)for(int i=0;i<path->npts*2;++i)if(!std::isfinite(path->pts[i])||std::abs(path->pts[i])>1000000)throw std::invalid_argument("Invalid SVG geometry coordinate");
    DecodedImage out;out.width=static_cast<std::uint32_t>(std::ceil(image->width));out.height=static_cast<std::uint32_t>(std::ceil(image->height));out.sourceChannels=4;
    out.rgba8.resize(static_cast<std::size_t>(out.width)*out.height*4);
    std::unique_ptr<NSVGrasterizer,decltype(&nsvgDeleteRasterizer)> raster(nsvgCreateRasterizer(),nsvgDeleteRasterizer);
    if(!raster)throw std::runtime_error("SVG rasterizer allocation failed");
    nsvgRasterize(raster.get(),image.get(),0,0,1,out.rgba8.data(),static_cast<int>(out.width),static_cast<int>(out.height),static_cast<int>(out.width*4));
    if(cancelled&&cancelled())throw std::runtime_error("SVG decode cancelled");
    out.sourceLosses={"SVG rasterized at intrinsic 96-DPI size; vector editing is not retained"};return out;
}
}
