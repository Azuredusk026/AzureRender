#include "assets/image/ImageCodec.hpp"
#include "assets/AssetTransfer.hpp"
#include <algorithm>
#include <cstring>
namespace azurerender {
namespace {
bool prefix(const std::vector<unsigned char>& b,std::initializer_list<unsigned char> p) {
    return b.size()>=p.size()&&std::equal(p.begin(),p.end(),b.begin());
}
bool isTiff(const std::vector<unsigned char>& b){return prefix(b,{'I','I',42,0})||prefix(b,{'M','M',0,42})||prefix(b,{'I','I',43,0})||prefix(b,{'M','M',0,43});}
bool isExr(const std::vector<unsigned char>& b){return prefix(b,{0x76,0x2f,0x31,0x01});}
bool isWebp(const std::vector<unsigned char>& b){return b.size()>=12&&prefix(b,{'R','I','F','F'})&&std::memcmp(b.data()+8,"WEBP",4)==0;}
}
void registerImageFormats(AssetFormatRegistry& registry) {
    for(const std::string id:{"png","jpeg","bmp","tga","ppm","hdr","exr","tiff","webp"}) {
        AssetFormatDescriptor format;format.id=id;format.label=id;format.domain=AssetDomain::Image;format.canRead=true;format.extensions={"."+id};
        if(id=="jpeg")format.extensions.push_back(".jpg");if(id=="tiff")format.extensions.push_back(".tif");
        format.features={"rgba","top-left","precision","color-space"};
        format.probe=[id](const std::vector<unsigned char>& b){
            if(id=="png")return prefix(b,{137,80,78,71,13,10,26,10});if(id=="jpeg")return prefix(b,{255,216,255});
            if(id=="bmp")return prefix(b,{'B','M'});if(id=="tiff")return isTiff(b);if(id=="exr")return isExr(b);if(id=="webp")return isWebp(b);
            if(id=="ppm")return b.size()>2&&b[0]=='P'&&(b[1]=='2'||b[1]=='3'||b[1]=='5'||b[1]=='6');if(id=="hdr")return prefix(b,{'#','?','R','A','D','I','A','N','C','E'})||prefix(b,{'#','?','R','G','B','E'});
            return b.size()>=18&&b[1]==0&&(b[2]==2||b[2]==3||b[2]==10||b[2]==11)&&(b[16]==8||b[16]==24||b[16]==32);
        };
        format.prepare=[](const AssetTransferRequest& request,const AssetCancellation& cancel){
            auto report=prepareAssetCopy(request,cancel);const auto image=decodeImageFile(request.staging/report.entry,cancel);
            const auto material=convertImageToRgba8(image);report.losses=material.losses;
            report.summary={{"domain","image"},{"width",image.width},{"height",image.height},{"channels",image.sourceChannels},
                {"precision",image.precision==ImagePrecision::UInt8?"uint8":image.precision==ImagePrecision::UInt16?"uint16":"float32"},
                {"colorSpace",image.colorSpace==ImageColorSpace::Linear?"linear":"srgb"},{"direction","top-left"},{"materialLosses",report.losses}};return report;
        };registry.add(std::move(format));
    }
}
}
