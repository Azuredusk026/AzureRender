#include "assets/image/ImageCodec.hpp"
#include "assets/AssetTransfer.hpp"
#include <stb_image.h>
#include <tiffio.h>
#include <webp/decode.h>
#include <OpenEXR/ImfInputFile.h>
#include <OpenEXR/ImfHeader.h>
#include <OpenEXR/ImfChannelList.h>
#include <OpenEXR/ImfFrameBuffer.h>
#include <OpenEXR/ImfIO.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <memory>
#include <stdexcept>

namespace azurerender {
namespace {
constexpr std::size_t sourceBudget=64ULL*1024*1024, pixelBudget=256ULL*1024*1024;
void check(const AssetCancellation& cancel){if(cancel&&cancel())throw std::runtime_error("Image decode cancelled");}
std::size_t sizeFor(std::int64_t w,std::int64_t h,std::size_t bytes) {
    if(w<1||h<1||w>8192||h>8192||static_cast<std::uint64_t>(w)*h*4*bytes>pixelBudget)
        throw std::invalid_argument("Image dimensions exceed 8192 or decoded pixels exceed 256 MiB");
    return static_cast<std::size_t>(w*h*4);
}
bool prefix(const std::vector<unsigned char>& b,std::initializer_list<unsigned char> p) {
    return b.size()>=p.size()&&std::equal(p.begin(),p.end(),b.begin());
}
bool isTiff(const std::vector<unsigned char>& b){return prefix(b,{'I','I',42,0})||prefix(b,{'M','M',0,42})||prefix(b,{'I','I',43,0})||prefix(b,{'M','M',0,43});}
bool isExr(const std::vector<unsigned char>& b){return prefix(b,{0x76,0x2f,0x31,0x01});}
bool isWebp(const std::vector<unsigned char>& b){return b.size()>=12&&prefix(b,{'R','I','F','F'})&&std::memcmp(b.data()+8,"WEBP",4)==0;}
bool isPnm(const std::vector<unsigned char>& b){return b.size()>2&&b[0]=='P'&&(b[1]=='2'||b[1]=='3'||b[1]=='5'||b[1]=='6');}
DecodedImage readPnm(const std::vector<unsigned char>& bytes,const AssetCancellation& cancel) {
    std::size_t pos=2;
    const auto white=[](unsigned char c){return c==' '||c=='\t'||c=='\r'||c=='\n'||c=='\f';};
    const auto number=[&](){
        for(;;){while(pos<bytes.size()&&white(bytes[pos]))++pos;if(pos<bytes.size()&&bytes[pos]=='#'){while(pos<bytes.size()&&bytes[pos]!='\n')++pos;}else break;}
        if(pos==bytes.size()||bytes[pos]<'0'||bytes[pos]>'9')throw std::runtime_error("Invalid PNM number");
        std::uint64_t value=0;while(pos<bytes.size()&&bytes[pos]>='0'&&bytes[pos]<='9'){value=value*10+bytes[pos++]-'0';if(value>65535)throw std::runtime_error("PNM value exceeds supported range");}return static_cast<unsigned>(value);
    };
    const auto w=number(),h=number(),maximum=number();if(maximum==0)throw std::runtime_error("Invalid PNM maximum");
    const bool gray=bytes[1]=='2'||bytes[1]=='5',binary=bytes[1]=='5'||bytes[1]=='6',wide=maximum>255;
    const auto count=sizeFor(w,h,wide?2:1);DecodedImage out;out.width=w;out.height=h;out.sourceChannels=gray?1:3;
    out.precision=wide?ImagePrecision::UInt16:ImagePrecision::UInt8;if(wide)out.rgba16.resize(count);else out.rgba8.resize(count);
    if(binary){if(pos>=bytes.size()||!white(bytes[pos]))throw std::runtime_error("Invalid PNM data separator");const auto separator=bytes[pos++];if(separator=='\r'&&pos<bytes.size()&&bytes[pos]=='\n')++pos;
        const auto need=static_cast<std::size_t>(w)*h*(gray?1:3)*(wide?2:1);if(bytes.size()-pos!=need)throw std::runtime_error("Truncated or extra PNM pixels");}
    for(std::size_t pixel=0;pixel<count/4;++pixel){if(pixel%w==0)check(cancel);std::array<unsigned,4> values{0,0,0,wide?65535U:255U};
        for(unsigned c=0;c<(gray?1U:3U);++c){unsigned v;if(binary){v=bytes[pos++];if(wide)v=(v<<8)|bytes[pos++];}else v=number();
            if(v>maximum)throw std::runtime_error("PNM sample exceeds maximum");values[c]=static_cast<unsigned>((static_cast<std::uint64_t>(v)*(wide?65535:255)+maximum/2)/maximum);}
        if(gray)values[1]=values[2]=values[0];for(unsigned c=0;c<4;++c){if(wide)out.rgba16[pixel*4+c]=static_cast<std::uint16_t>(values[c]);else out.rgba8[pixel*4+c]=static_cast<std::uint8_t>(values[c]);}
    }return out;
}
struct Memory {
    const std::vector<unsigned char>& bytes;std::size_t position=0;
};
tmsize_t tiffRead(thandle_t handle,void* out,tmsize_t count){auto& m=*static_cast<Memory*>(handle);if(count<0)return 0;const auto n=std::min(static_cast<std::size_t>(count),m.bytes.size()-m.position);std::memcpy(out,m.bytes.data()+m.position,n);m.position+=n;return static_cast<tmsize_t>(n);}
tmsize_t tiffWrite(thandle_t,void*,tmsize_t){return 0;}
toff_t tiffSeek(thandle_t handle,toff_t offset,int origin){auto& m=*static_cast<Memory*>(handle);const auto base=origin==SEEK_SET?0:origin==SEEK_CUR?m.position:m.bytes.size();const auto delta=static_cast<std::int64_t>(offset);if(delta<0&&static_cast<std::uint64_t>(-delta)>base)return static_cast<toff_t>(-1);const auto pos=delta<0?base-static_cast<std::size_t>(-delta):base+static_cast<std::size_t>(delta);if(pos>m.bytes.size())return static_cast<toff_t>(-1);m.position=pos;return static_cast<toff_t>(pos);}
int tiffClose(thandle_t){return 0;}
toff_t tiffSize(thandle_t handle){return static_cast<toff_t>(static_cast<Memory*>(handle)->bytes.size());}
int tiffMap(thandle_t,void**,toff_t*){return 0;}
void tiffUnmap(thandle_t,void*,toff_t){}
int tiffDiagnostic(TIFF*,void*,const char*,const char*,va_list){return 1;}
DecodedImage readTiff(const std::vector<unsigned char>& bytes,const AssetCancellation& cancel) {
    Memory memory{bytes};
    std::unique_ptr<TIFFOpenOptions,decltype(&TIFFOpenOptionsFree)> options(TIFFOpenOptionsAlloc(),TIFFOpenOptionsFree);
    TIFFOpenOptionsSetMaxSingleMemAlloc(options.get(),static_cast<tmsize_t>(pixelBudget));
    TIFFOpenOptionsSetErrorHandlerExtR(options.get(),tiffDiagnostic,nullptr);
    TIFFOpenOptionsSetWarningHandlerExtR(options.get(),tiffDiagnostic,nullptr);
    std::unique_ptr<TIFF,decltype(&TIFFClose)> file(TIFFClientOpenExt("memory","rm",&memory,tiffRead,tiffWrite,tiffSeek,tiffClose,tiffSize,tiffMap,tiffUnmap,options.get()),TIFFClose);
    if(!file)throw std::runtime_error("Invalid TIFF container");
    std::uint32_t w=0,h=0;std::uint16_t bits=0,samples=0,photo=0,planar=0,orientation=0,kind=0;
    TIFFGetField(file.get(),TIFFTAG_IMAGEWIDTH,&w);TIFFGetField(file.get(),TIFFTAG_IMAGELENGTH,&h);
    TIFFGetFieldDefaulted(file.get(),TIFFTAG_BITSPERSAMPLE,&bits);TIFFGetFieldDefaulted(file.get(),TIFFTAG_SAMPLESPERPIXEL,&samples);
    TIFFGetField(file.get(),TIFFTAG_PHOTOMETRIC,&photo);TIFFGetFieldDefaulted(file.get(),TIFFTAG_PLANARCONFIG,&planar);
    TIFFGetFieldDefaulted(file.get(),TIFFTAG_ORIENTATION,&orientation);TIFFGetFieldDefaulted(file.get(),TIFFTAG_SAMPLEFORMAT,&kind);
    if(TIFFIsTiled(file.get())||planar!=PLANARCONFIG_CONTIG||orientation<1||orientation>4||
       (photo!=PHOTOMETRIC_RGB&&photo!=PHOTOMETRIC_MINISBLACK&&photo!=PHOTOMETRIC_MINISWHITE)||
       samples<1||samples>4||(photo==PHOTOMETRIC_RGB&&samples<3)||
       !((kind==SAMPLEFORMAT_UINT&&(bits==8||bits==16))||(kind==SAMPLEFORMAT_IEEEFP&&bits==32)))
        throw std::invalid_argument("Unsupported TIFF pixel layout (contiguous integer/float RGB or gray required)");
    DecodedImage out;out.width=w;out.height=h;out.sourceChannels=samples;
    const auto count=sizeFor(w,h,bits/8);
    out.precision=bits==8?ImagePrecision::UInt8:bits==16?ImagePrecision::UInt16:ImagePrecision::Float32;
    out.colorSpace=bits==32?ImageColorSpace::Linear:ImageColorSpace::Srgb;
    if(bits==8)out.rgba8.resize(count);else if(bits==16)out.rgba16.resize(count);else out.rgba32.resize(count);
    const auto scan=TIFFScanlineSize64(file.get());
    if(scan>pixelBudget||scan<static_cast<std::uint64_t>(w)*samples*(bits/8))throw std::runtime_error("Invalid TIFF scanline budget");
    std::vector<unsigned char> row(static_cast<std::size_t>(scan));
    std::uint16_t extraCount=0;std::uint16_t* extra=nullptr;
    TIFFGetField(file.get(),TIFFTAG_EXTRASAMPLES,&extraCount,&extra);
    const bool alpha=samples==(photo==PHOTOMETRIC_RGB?4:2)&&extraCount==1&&extra;
    const bool associated=alpha&&extra[0]==EXTRASAMPLE_ASSOCALPHA;
    const double maximum=bits==8?255.:bits==16?65535.:1.;
    for(std::uint32_t y=0;y<h;++y){check(cancel);if(TIFFReadScanline(file.get(),row.data(),y,0)<0)throw std::runtime_error("Truncated TIFF scanline");
        for(std::uint32_t x=0;x<w;++x){std::array<double,4> value{0,0,0,maximum};
            for(std::uint16_t c=0;c<samples;++c){const auto at=(static_cast<std::size_t>(x)*samples+c)*(bits/8);double sample=0;
                if(bits==8)sample=row[at];else if(bits==16){std::uint16_t v;std::memcpy(&v,row.data()+at,2);sample=v;}else{float v;std::memcpy(&v,row.data()+at,4);sample=v;}
                if(photo==PHOTOMETRIC_RGB)value[c]=sample;else if(c==0)value[0]=value[1]=value[2]=photo==PHOTOMETRIC_MINISWHITE?maximum-sample:sample;else if(c==1&&alpha)value[3]=sample;
            }
            if(!alpha)value[3]=maximum;
            if(associated)for(int c=0;c<3;++c)value[c]=value[3]>0?value[c]*maximum/value[3]:0;
            const auto dx=orientation==2||orientation==3?w-1-x:x;const auto dy=orientation==3||orientation==4?h-1-y:y;
            const auto at=(static_cast<std::size_t>(dy)*w+dx)*4;
            for(int c=0;c<4;++c){if(!std::isfinite(value[c]))throw std::runtime_error("Non-finite TIFF pixels");
                if(bits==8)out.rgba8[at+c]=static_cast<std::uint8_t>(std::clamp(value[c],0.,maximum));
                else if(bits==16)out.rgba16[at+c]=static_cast<std::uint16_t>(std::clamp(value[c],0.,maximum));else out.rgba32[at+c]=static_cast<float>(value[c]);}
        }
    }return out;
}
class ExrStream final:public Imf::IStream {
    Memory memory_;
public:
    explicit ExrStream(const std::vector<unsigned char>& b):Imf::IStream("memory"),memory_{b}{}
    bool read(char out[],int n) override {if(n<0||static_cast<std::size_t>(n)>memory_.bytes.size()-memory_.position)throw std::runtime_error("Truncated EXR");std::memcpy(out,memory_.bytes.data()+memory_.position,n);memory_.position+=n;return memory_.position<memory_.bytes.size();}
    std::uint64_t tellg() override{return memory_.position;}
    void seekg(std::uint64_t p) override{if(p>memory_.bytes.size())throw std::runtime_error("Invalid EXR seek");memory_.position=static_cast<std::size_t>(p);}
};
DecodedImage readExr(const std::vector<unsigned char>& bytes,const AssetCancellation& cancel) {
    ExrStream stream(bytes);Imf::InputFile file(stream);const auto window=file.header().dataWindow();
    const auto w=static_cast<std::int64_t>(window.max.x)-window.min.x+1,h=static_cast<std::int64_t>(window.max.y)-window.min.y+1;
    const auto count=sizeFor(w,h,sizeof(float));DecodedImage out;out.width=static_cast<std::uint32_t>(w);out.height=static_cast<std::uint32_t>(h);
    out.precision=ImagePrecision::Float32;out.colorSpace=ImageColorSpace::Linear;out.rgba32.resize(count);Imf::FrameBuffer frame;
    const auto& channels=file.header().channels();const bool gray=!channels.findChannel("R")&&channels.findChannel("Y");
    if(!gray&&(!channels.findChannel("R")||!channels.findChannel("G")||!channels.findChannel("B")))throw std::runtime_error("EXR requires RGB or Y channels");
    out.sourceChannels=gray?1:3;if(channels.findChannel("A"))++out.sourceChannels;
    for(int c=0;c<4;++c)frame.insert(c==3?"A":gray?"Y":c==0?"R":c==1?"G":"B",Imf::Slice::Make(Imf::FLOAT,out.rgba32.data()+c,window,4*sizeof(float),static_cast<std::size_t>(w)*4*sizeof(float),1,1,c==3?1.:0.));
    // A grayscale channel has one slice; replicate it after reading.
    file.setFrameBuffer(frame);for(int y=window.min.y;;++y){check(cancel);file.readPixels(y);if(y==window.max.y)break;}
    if(gray)for(std::size_t i=0;i<count;i+=4)out.rgba32[i]=out.rgba32[i+1]=out.rgba32[i+2];
    for(float v:out.rgba32)if(!std::isfinite(v))throw std::runtime_error("Non-finite EXR pixels");
    return out;
}
}
DecodedImage decodeImageBytes(const std::vector<unsigned char>& bytes,const AssetCancellation& cancel) {
    check(cancel);if(bytes.empty()||bytes.size()>sourceBudget)throw std::invalid_argument("Image source exceeds the 64 MiB budget or is empty");
    if(isTiff(bytes))return readTiff(bytes,cancel);if(isExr(bytes))return readExr(bytes,cancel);if(isPnm(bytes))return readPnm(bytes,cancel);
    DecodedImage out;int w=0,h=0,c=0;const auto n=static_cast<int>(bytes.size());
    if(isWebp(bytes)) {
        if(!WebPGetInfo(bytes.data(),bytes.size(),&w,&h))throw std::runtime_error("Invalid WebP image");
        const auto count=sizeFor(w,h,1);out.rgba8.resize(count);
        WebPBitstreamFeatures features{};if(WebPGetFeatures(bytes.data(),bytes.size(),&features)!=VP8_STATUS_OK||features.has_animation)throw std::invalid_argument("Animated or invalid WebP requires a frame policy");
        if(!WebPDecodeRGBAInto(bytes.data(),bytes.size(),out.rgba8.data(),count,w*4))throw std::runtime_error("Truncated WebP image");c=features.has_alpha?4:3;
    } else {
        if(!stbi_info_from_memory(bytes.data(),n,&w,&h,&c))throw std::runtime_error("Unsupported or truncated image container");
        if(stbi_is_hdr_from_memory(bytes.data(),n)) {
            const auto count=sizeFor(w,h,4);out.precision=ImagePrecision::Float32;out.colorSpace=ImageColorSpace::Linear;
            std::unique_ptr<float,decltype(&stbi_image_free)> data(stbi_loadf_from_memory(bytes.data(),n,&w,&h,&c,4),stbi_image_free);
            if(!data)throw std::runtime_error("Cannot decode HDR pixels");out.rgba32.assign(data.get(),data.get()+count);
            for(float v:out.rgba32)if(!std::isfinite(v))throw std::runtime_error("Non-finite HDR pixels");
        } else if(stbi_is_16_bit_from_memory(bytes.data(),n)) {
            const auto count=sizeFor(w,h,2);out.precision=ImagePrecision::UInt16;
            std::unique_ptr<stbi_us,decltype(&stbi_image_free)> data(stbi_load_16_from_memory(bytes.data(),n,&w,&h,&c,4),stbi_image_free);
            if(!data)throw std::runtime_error("Cannot decode sixteen-bit pixels");out.rgba16.assign(data.get(),data.get()+count);
        } else {
            const auto count=sizeFor(w,h,1);std::unique_ptr<stbi_uc,decltype(&stbi_image_free)> data(stbi_load_from_memory(bytes.data(),n,&w,&h,&c,4),stbi_image_free);
            if(!data)throw std::runtime_error("Cannot decode image pixels");out.rgba8.assign(data.get(),data.get()+count);
        }
    }
    check(cancel);out.width=static_cast<std::uint32_t>(w);out.height=static_cast<std::uint32_t>(h);out.sourceChannels=static_cast<std::uint32_t>(c);return out;
}
DecodedImage decodeImageFile(const std::filesystem::path& path,const AssetCancellation& cancel) {
    try {check(cancel);std::ifstream input(path,std::ios::binary|std::ios::ate);if(!input)throw std::runtime_error("Cannot open image");
        const auto size=input.tellg();if(size<=0||size>static_cast<std::streamoff>(sourceBudget))throw std::invalid_argument("Image source exceeds the 64 MiB budget or is empty");
        std::vector<unsigned char> bytes(static_cast<std::size_t>(size));input.seekg(0);input.read(reinterpret_cast<char*>(bytes.data()),size);if(!input)throw std::runtime_error("Incomplete image source");return decodeImageBytes(bytes,cancel);
    }catch(const std::exception& e){auto ext=path.extension().u8string();std::transform(ext.begin(),ext.end(),ext.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});throw std::runtime_error(std::string("Cannot decode ")+(ext==".exr"?"EXR image ":"image ")+path.u8string()+": "+e.what());}
}
ImageRgba8 convertImageToRgba8(const DecodedImage& image,bool encodeLinearRgb) {
    const auto count=sizeFor(image.width,image.height,1);ImageRgba8 out;
    if(image.precision==ImagePrecision::UInt8){if(image.rgba8.size()!=count)throw std::invalid_argument("Invalid RGBA image size");out.pixels=image.rgba8;return out;}
    if((image.precision==ImagePrecision::UInt16?image.rgba16.size():image.rgba32.size())!=count)throw std::invalid_argument("Invalid RGBA image size");
    out.pixels.resize(count);out.losses.push_back("Pixel precision converted to UInt8");bool clipped=false;
    for(std::size_t i=0;i<count;++i){float v=image.precision==ImagePrecision::UInt16?image.rgba16[i]/65535.F:image.rgba32[i];
        if(!std::isfinite(v))throw std::invalid_argument("Non-finite image pixels");clipped=clipped||v<0||v>1;v=std::clamp(v,0.F,1.F);
        if(encodeLinearRgb&&image.colorSpace==ImageColorSpace::Linear&&i%4!=3)v=v<=.0031308F?12.92F*v:1.055F*std::pow(v,1.F/2.4F)-.055F;
        out.pixels[i]=static_cast<std::uint8_t>(std::lround(v*255));}
    if(clipped)out.losses.push_back("Values outside [0,1] clipped for material storage");return out;
}
std::vector<float> imageLinearRgba(const DecodedImage& image) {
    const auto count=sizeFor(image.width,image.height,4);std::vector<float> out(count);
    if((image.precision==ImagePrecision::UInt8?image.rgba8.size():image.precision==ImagePrecision::UInt16?image.rgba16.size():image.rgba32.size())!=count)throw std::invalid_argument("Invalid RGBA image size");
    for(std::size_t i=0;i<count;++i){float v=image.precision==ImagePrecision::UInt8?image.rgba8[i]/255.F:image.precision==ImagePrecision::UInt16?image.rgba16[i]/65535.F:image.rgba32[i];
        if(image.colorSpace==ImageColorSpace::Srgb&&i%4!=3)v=v<=.04045F?v/12.92F:std::pow((v+.055F)/1.055F,2.4F);out[i]=v;}return out;
}
}
