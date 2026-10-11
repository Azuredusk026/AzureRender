#include "assets/AssetFormatRegistry.hpp"
#include "assets/AssetTransfer.hpp"
#include "assets/GltfLoader.hpp"
#include "assets/image/ImageCodec.hpp"
#include "render/EnvironmentAsset.hpp"
#include <stb_image_write.h>
#include <tiffio.h>
#include <webp/encode.h>
#include <OpenEXR/ImfOutputFile.h>
#include <OpenEXR/ImfHeader.h>
#include <OpenEXR/ImfChannelList.h>
#include <OpenEXR/ImfFrameBuffer.h>
#include <array>
#include <fstream>
#include <cmath>
#include <chrono>
#include <iostream>
#include <stdexcept>
using namespace azurerender;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class F> void rejects(F&& f,const char* message){bool rejected=false;try{f();}catch(const std::exception&){rejected=true;}require(rejected,message);}
void writeTiff(const std::filesystem::path& path,int bits,int orientation=1){
    TIFF* file=TIFFOpen(path.string().c_str(),"w");require(file!=nullptr,"Create TIFF fixture");
    TIFFSetField(file,TIFFTAG_IMAGEWIDTH,2U);TIFFSetField(file,TIFFTAG_IMAGELENGTH,1U);
    TIFFSetField(file,TIFFTAG_BITSPERSAMPLE,bits);TIFFSetField(file,TIFFTAG_SAMPLESPERPIXEL,1);
    TIFFSetField(file,TIFFTAG_PHOTOMETRIC,PHOTOMETRIC_MINISBLACK);TIFFSetField(file,TIFFTAG_PLANARCONFIG,PLANARCONFIG_CONTIG);
    TIFFSetField(file,TIFFTAG_ORIENTATION,orientation);TIFFSetField(file,TIFFTAG_SAMPLEFORMAT,bits==32?SAMPLEFORMAT_IEEEFP:SAMPLEFORMAT_UINT);
    std::array<std::uint16_t,2> u16{0,32768};std::array<float,2> f32{128.F,-.5F};
    require(TIFFWriteScanline(file,bits==16?static_cast<void*>(u16.data()):static_cast<void*>(f32.data()),0,0)==1,"Write TIFF pixels");TIFFClose(file);
}
void writeExr(const std::filesystem::path& path){
    const Imath::Box2i window({-3,7},{-2,7});Imf::Header header(2,1);header.dataWindow()=window;
    for(const char* name:{"R","G","B","A"})header.channels().insert(name,Imf::Channel(Imf::FLOAT));
    std::array<float,8> pixels{128.F,.5F,-.25F,.5F,2.F,4.F,8.F,1.F};Imf::FrameBuffer frame;
    const char* names[]={"R","G","B","A"};for(int c=0;c<4;++c)frame.insert(names[c],Imf::Slice::Make(Imf::FLOAT,pixels.data()+c,window,4*sizeof(float),8*sizeof(float)));
    Imf::OutputFile file(path.string().c_str(),header);file.setFrameBuffer(frame);file.writePixels(1);
}
}
int main(int argc,char** argv){const auto root=std::filesystem::temp_directory_path()/("azure-image-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));try{
    std::filesystem::create_directories(root);
    const auto& formats=azurerender::assetFormatRegistry();
    for(const std::string id:{"png","jpeg","bmp","tga","ppm","hdr","exr","tiff","webp"})
        if(!formats.require(id).canRead)throw std::runtime_error("Image formats require production readers");
    const std::array<unsigned char,16> pixels{255,0,0,128,0,255,0,255,0,0,255,255,255,255,255,0};
    require(stbi_write_png((root/"alpha.png").string().c_str(),2,2,4,pixels.data(),8)!=0,"Create PNG");
    require(stbi_write_bmp((root/"color.bmp").string().c_str(),2,2,4,pixels.data())!=0,"Create BMP");
    require(stbi_write_tga((root/"color.tga").string().c_str(),2,2,4,pixels.data())!=0,"Create TGA");
    const std::array<unsigned char,12> rgb{255,0,0,0,255,0,0,0,255,255,255,255};
    require(stbi_write_jpg((root/"color.jpg").string().c_str(),2,2,3,rgb.data(),100)!=0,"Create JPEG");
    {std::ofstream file(root/"color.ppm",std::ios::binary);file<<"P6\n2 2\n255\n";file.write(reinterpret_cast<const char*>(rgb.data()),rgb.size());}
    {std::ofstream file(root/"gray16.ppm",std::ios::binary);file<<"P5\n2 1\n65535\n";const char data[]={0,0,static_cast<char>(128),0};file.write(data,4);}
    const std::array<float,6> hdr{128,.5F,.25F,2,4,8};require(stbi_write_hdr((root/"linear.hdr").string().c_str(),2,1,3,hdr.data())!=0,"Create HDR");
    {const unsigned char bytes[]={137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,2,0,0,0,1,16,0,0,0,0,129,217,252,21,0,0,0,13,73,68,65,84,120,156,99,16,50,89,125,22,0,3,12,1,191,110,185,198,93,0,0,0,0,73,69,78,68,174,66,96,130};std::ofstream file(root/"gray16.png",std::ios::binary);file.write(reinterpret_cast<const char*>(bytes),sizeof(bytes));}
    writeTiff(root/"gray16.tiff",16,2);writeTiff(root/"linear.tiff",32);writeExr(root/"linear.exr");
    WebPConfig config;require(WebPConfigInit(&config)!=0,"Initialize WebP fixture encoder");config.lossless=1;config.exact=1;
    WebPPicture picture;require(WebPPictureInit(&picture)!=0,"Initialize WebP fixture image");picture.width=2;picture.height=2;picture.use_argb=1;
    WebPMemoryWriter writer;WebPMemoryWriterInit(&writer);picture.writer=WebPMemoryWrite;picture.custom_ptr=&writer;
    require(WebPPictureImportRGBA(&picture,pixels.data(),8)!=0&&WebPEncode(&config,&picture)!=0,"Create WebP preserving RGB under transparent alpha");
    {std::ofstream file(root/"color.webp",std::ios::binary);file.write(reinterpret_cast<const char*>(writer.mem),writer.size);}WebPPictureFree(&picture);WebPMemoryWriterClear(&writer);
    for(const auto& entry:std::filesystem::directory_iterator(root)) {
        const auto image=decodeImageFile(entry.path());require(image.width==2&&(image.height==1||image.height==2),"Decoded dimensions");
        const auto& format=formats.detect(entry.path());require(format.domain==AssetDomain::Image,"Content-aware image discovery");
        AssetTransferRequest request;request.source=entry.path();request.staging=root/("stage-"+entry.path().filename().string());request.destination=root/("published-"+entry.path().filename().string());
        auto report=prepareAssetTransfer(request,formats,{});require(report.summary.at("domain")=="image","Image transfer summary");
        const auto target=publishAssetTransfer(report);require(decodeImageFile(target).width==2,"Publication retains a decodable source");
        std::filesystem::remove_all(request.destination);
    }
    const auto png=decodeImageFile(root/"alpha.png");require(png.rgba8==std::vector<unsigned char>(pixels.begin(),pixels.end()),"PNG alpha and top-left order");
    require(decodeImageFile(root/"color.webp").rgba8==png.rgba8,"Lossless WebP alpha and row order");
    const auto png16=decodeImageFile(root/"gray16.png");require(png16.precision==ImagePrecision::UInt16&&png16.rgba16[0]==0x1234&&png16.rgba16[4]==0xabcd,"PNG preserves sixteen-bit grayscale values");
    const auto tiff=decodeImageFile(root/"gray16.tiff");require(tiff.precision==ImagePrecision::UInt16&&tiff.rgba16==std::vector<std::uint16_t>{32768,32768,32768,65535,0,0,0,65535},"TIFF sixteen-bit grayscale and orientation");
    const auto gray=decodeImageFile(root/"gray16.ppm");require(gray.precision==ImagePrecision::UInt16&&gray.rgba16[4]==32768,"PNM preserves sixteen-bit big-endian samples");
    const auto exr=decodeImageFile(root/"linear.exr");require(exr.precision==ImagePrecision::Float32&&exr.colorSpace==ImageColorSpace::Linear&&exr.rgba32==std::vector<float>{128,.5F,-.25F,.5F,2,4,8,1},"EXR preserves negative data window, range and alpha");
    const auto converted=convertImageToRgba8(exr);require(converted.pixels[0]==255&&converted.pixels[1]==188&&converted.pixels[2]==0&&converted.pixels[3]==128&&converted.losses.size()==2,"Material conversion names precision and clipping loss; alpha stays linear");
    require(imageLinearRgba(png)[3]==128/255.F,"Linear color conversion preserves alpha");
    require(decodeImageFile(root/"linear.hdr").rgba32[0]==128,"HDR range above 64 remains intact");
    rejects([&]{decodeImageFile(root/"alpha.png",[]{return true;});},"Cancellation refuses decode");
    rejects([&]{decodeImageBytes({1,2,3});},"Malformed input is rejected");
    std::vector<unsigned char> hugeTga(18);hugeTga[2]=2;hugeTga[12]=1;hugeTga[13]=32;hugeTga[14]=1;hugeTga[16]=24;
    rejects([&]{decodeImageBytes(hugeTga);},"Oversize dimensions fail before allocation");
    std::ifstream source(root/"color.webp",std::ios::binary);std::vector<unsigned char> truncated((std::istreambuf_iterator<char>(source)),{});source.close();truncated.resize(truncated.size()/2);
    rejects([&]{decodeImageBytes(truncated);},"Truncated WebP is rejected");
    const auto utf8=root/std::filesystem::u8path(u8"图像 灰度.TIFF");std::filesystem::copy_file(root/"gray16.tiff",utf8);
    const auto material=loadAssetImage(utf8.u8string());require(material.width==2&&material.pixels[0]==128,"Material loader accepts Unicode TIFF and explicitly converts precision");
    const auto env=loadEnvironmentImage({(root/"linear.hdr").u8string()});require(env.rgba16f[0]==environmentFloatToHalf(128),"Environment range above 64 remains intact");
    if(argc==2){const auto output=std::filesystem::u8path(argv[1]);std::filesystem::create_directories(output);for(const auto& file:std::filesystem::directory_iterator(root))if(file.is_regular_file())std::filesystem::copy_file(file.path(),output/file.path().filename(),std::filesystem::copy_options::overwrite_existing);}
    std::filesystem::remove_all(root);std::cout<<"Image readers, precision, orientation, transfer, material and environment passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';std::filesystem::remove_all(root);return 1;}}
