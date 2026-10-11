#include "assets/image/ImageEncoder.hpp"
#include "assets/image/ImageCodec.hpp"
#include <stb_image_write.h>
#include <tiffio.h>
#include <webp/encode.h>
#include <zlib.h>
#include <OpenEXR/ImfOutputFile.h>
#include <OpenEXR/ImfHeader.h>
#include <OpenEXR/ImfChannelList.h>
#include <OpenEXR/ImfFrameBuffer.h>
#include <OpenEXR/ImfIO.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <memory>
#include <stdexcept>
namespace azurerender {namespace {
constexpr std::size_t outputBudget=64ULL*1024*1024;
void check(const AssetCancellation& cancel){if(cancel&&cancel())throw std::runtime_error("Image export cancelled");}
struct Bytes{std::vector<unsigned char> value;AssetCancellation cancel;};
void append(void* context,void* data,int count){auto& out=*static_cast<Bytes*>(context);check(out.cancel);if(count<0||static_cast<std::size_t>(count)>outputBudget-out.value.size())throw std::runtime_error("Encoded image exceeds 64 MiB");const auto* bytes=static_cast<unsigned char*>(data);out.value.insert(out.value.end(),bytes,bytes+count);}
void big(std::vector<unsigned char>& out,std::uint32_t value){for(int shift=24;shift>=0;shift-=8)out.push_back(static_cast<unsigned char>(value>>shift));}
void chunk(std::vector<unsigned char>& out,const char* type,const std::vector<unsigned char>& data){big(out,static_cast<std::uint32_t>(data.size()));const auto at=out.size();out.insert(out.end(),type,type+4);out.insert(out.end(),data.begin(),data.end());big(out,static_cast<std::uint32_t>(crc32(0,out.data()+at,static_cast<uInt>(data.size()+4))));}
std::vector<unsigned char> png16(const DecodedImage& image,const AssetCancellation& cancel){
    std::vector<unsigned char> raw;raw.reserve((static_cast<std::size_t>(image.width)*8+1)*image.height);
    for(unsigned y=0;y<image.height;++y){check(cancel);raw.push_back(0);for(std::size_t i=static_cast<std::size_t>(y)*image.width*4;i<static_cast<std::size_t>(y+1)*image.width*4;++i){raw.push_back(static_cast<unsigned char>(image.rgba16[i]>>8));raw.push_back(static_cast<unsigned char>(image.rgba16[i]));}}
    uLongf count=static_cast<uLongf>(std::min<std::size_t>(compressBound(static_cast<uLong>(raw.size())),outputBudget-128));std::vector<unsigned char> compressed(count);
    if(compress2(compressed.data(),&count,raw.data(),static_cast<uLong>(raw.size()),6)!=Z_OK)throw std::runtime_error("PNG encoding failed or exceeds its output budget");compressed.resize(count);check(cancel);
    std::vector<unsigned char> bytes{137,80,78,71,13,10,26,10};std::vector<unsigned char> header;big(header,image.width);big(header,image.height);header.insert(header.end(),{16,6,0,0,0});chunk(bytes,"IHDR",header);chunk(bytes,"IDAT",compressed);chunk(bytes,"IEND",{});return bytes;
}
class ExrOutput final:public Imf::OStream {
    std::ofstream output_;AssetCancellation cancel_;
public:
    ExrOutput(const std::filesystem::path& path,AssetCancellation cancel):Imf::OStream("native-file"),output_(path,std::ios::binary),cancel_(std::move(cancel)){if(!output_)throw std::runtime_error("EXR destination unavailable");}
    void write(const char bytes[],int count)override{check(cancel_);const auto at=tellp();if(count<0||static_cast<std::uint64_t>(count)>outputBudget-at)throw std::runtime_error("EXR exceeds its byte budget");output_.write(bytes,count);if(!output_)throw std::runtime_error("EXR write failed");}
    std::uint64_t tellp()override{const auto position=output_.tellp();if(position<0)throw std::runtime_error("EXR tell failed");return static_cast<std::uint64_t>(position);}
    void seekp(std::uint64_t position)override{if(position>outputBudget)throw std::runtime_error("EXR seek exceeds budget");output_.seekp(static_cast<std::streamoff>(position));if(!output_)throw std::runtime_error("EXR seek failed");}
};
int diagnostic(TIFF*,void*,const char*,const char*,va_list){return 1;}
void tiff(const std::filesystem::path& path,const DecodedImage& image,const AssetCancellation& cancel){
    std::unique_ptr<TIFFOpenOptions,decltype(&TIFFOpenOptionsFree)> options(TIFFOpenOptionsAlloc(),TIFFOpenOptionsFree);TIFFOpenOptionsSetMaxSingleMemAlloc(options.get(),outputBudget);TIFFOpenOptionsSetErrorHandlerExtR(options.get(),diagnostic,nullptr);TIFFOpenOptionsSetWarningHandlerExtR(options.get(),diagnostic,nullptr);
#ifdef _WIN32
    TIFF* opened=TIFFOpenWExt(path.c_str(),"w",options.get());
#else
    TIFF* opened=TIFFOpenExt(path.c_str(),"w",options.get());
#endif
    std::unique_ptr<TIFF,decltype(&TIFFClose)> file(opened,TIFFClose);if(!file)throw std::runtime_error("TIFF output unavailable");
    const unsigned bits=image.precision==ImagePrecision::UInt8?8:image.precision==ImagePrecision::UInt16?16:32;
    std::uint16_t alpha=EXTRASAMPLE_UNASSALPHA;
    if(!TIFFSetField(file.get(),TIFFTAG_IMAGEWIDTH,image.width)||!TIFFSetField(file.get(),TIFFTAG_IMAGELENGTH,image.height)||!TIFFSetField(file.get(),TIFFTAG_SAMPLESPERPIXEL,4)||!TIFFSetField(file.get(),TIFFTAG_BITSPERSAMPLE,bits)||!TIFFSetField(file.get(),TIFFTAG_SAMPLEFORMAT,bits==32?SAMPLEFORMAT_IEEEFP:SAMPLEFORMAT_UINT)||!TIFFSetField(file.get(),TIFFTAG_PHOTOMETRIC,PHOTOMETRIC_RGB)||!TIFFSetField(file.get(),TIFFTAG_PLANARCONFIG,PLANARCONFIG_CONTIG)||!TIFFSetField(file.get(),TIFFTAG_ORIENTATION,ORIENTATION_TOPLEFT)||!TIFFSetField(file.get(),TIFFTAG_EXTRASAMPLES,1,&alpha)||!TIFFSetField(file.get(),TIFFTAG_ROWSPERSTRIP,16)||!TIFFSetField(file.get(),TIFFTAG_COMPRESSION,COMPRESSION_ADOBE_DEFLATE))throw std::runtime_error("TIFF output configuration failed");
    for(unsigned y=0;y<image.height;++y){check(cancel);const auto offset=static_cast<std::size_t>(y)*image.width*4;const void* row=bits==8?static_cast<const void*>(image.rgba8.data()+offset):bits==16?static_cast<const void*>(image.rgba16.data()+offset):static_cast<const void*>(image.rgba32.data()+offset);
        if(TIFFWriteScanline(file.get(),const_cast<void*>(row),y,0)<0)throw std::runtime_error("TIFF scanline write failed");}
    if(!TIFFWriteDirectory(file.get()))throw std::runtime_error("TIFF directory write failed");
}
}
AssetTransferReport prepareImageExport(const AssetTransferRequest& request,const AssetCancellation& cancel){
    const auto settings=normalizeImageExportSettings(request.formatId,request.settings);const int quality=settings.value("quality",100);
    const auto image=decodeImageFile(request.source,cancel);AssetTransferReport report;report.entry=request.source.stem();report.entry+="."+request.formatId;const auto target=request.staging/report.entry;
    if(request.formatId=="tiff")tiff(target,image,cancel);
    else if(request.formatId=="exr"){
        auto pixels=imageLinearRgba(image);Imf::Header header(image.width,image.height);const char* names[]={"R","G","B","A"};for(auto name:names)header.channels().insert(name,Imf::Channel(Imf::FLOAT));Imf::FrameBuffer frame;
        for(int c=0;c<4;++c)frame.insert(names[c],Imf::Slice(Imf::FLOAT,reinterpret_cast<char*>(pixels.data()+c),4*sizeof(float),static_cast<std::size_t>(image.width)*4*sizeof(float)));
        ExrOutput stream(target,cancel);Imf::OutputFile file(stream,header);file.setFrameBuffer(frame);for(unsigned y=0;y<image.height;++y){check(cancel);file.writePixels(1);}
    }else{
        Bytes bytes{{},cancel};int status=0;
        if(request.formatId=="png"&&image.precision==ImagePrecision::UInt16){bytes.value=png16(image,cancel);status=1;}
        else if(request.formatId=="hdr"){
            auto pixels=imageLinearRgba(image);for(std::size_t i=0;i<pixels.size();++i)if(i%4!=3&&pixels[i]<0){pixels[i]=0;report.losses.push_back("Negative radiance is clipped to zero");break;}
            for(std::size_t i=0;i<pixels.size();++i)if(i%4!=3)pixels[i]=std::max(0.F,pixels[i]);status=stbi_write_hdr_to_func(append,&bytes,image.width,image.height,4,pixels.data());report.losses.push_back("HDR stores RGB radiance with shared-exponent precision and omits alpha");
        }else{
            const auto converted=convertImageToRgba8(image);report.losses=converted.losses;const auto* pixels=converted.pixels.data();
            if(request.formatId=="png")status=stbi_write_png_to_func(append,&bytes,image.width,image.height,4,pixels,image.width*4);
            else if(request.formatId=="jpeg"){status=stbi_write_jpg_to_func(append,&bytes,image.width,image.height,4,pixels,quality);report.losses.push_back("JPEG uses lossy RGB coding and omits alpha");}
            else if(request.formatId=="bmp")status=stbi_write_bmp_to_func(append,&bytes,image.width,image.height,4,pixels);
            else if(request.formatId=="tga")status=stbi_write_tga_to_func(append,&bytes,image.width,image.height,4,pixels);
            else if(request.formatId=="webp"){
                WebPConfig config;if(!WebPConfigInit(&config))throw std::runtime_error("WebP configuration failed");config.lossless=1;config.exact=1;config.quality=static_cast<float>(quality);
                WebPPicture picture;if(!WebPPictureInit(&picture))throw std::runtime_error("WebP image initialization failed");picture.width=image.width;picture.height=image.height;picture.use_argb=1;
                WebPMemoryWriter writer;WebPMemoryWriterInit(&writer);picture.writer=WebPMemoryWrite;picture.custom_ptr=&writer;
                status=WebPPictureImportRGBA(&picture,pixels,image.width*4)&&WebPEncode(&config,&picture);if(status&&writer.size<=outputBudget)bytes.value.assign(writer.mem,writer.mem+writer.size);else status=0;WebPPictureFree(&picture);WebPMemoryWriterClear(&writer);check(cancel);
            }
        }
        if(!status||bytes.value.empty())throw std::runtime_error("Image encoding failed or exceeds its byte budget");std::ofstream output(target,std::ios::binary);for(std::size_t offset=0;offset<bytes.value.size();){check(cancel);const auto count=std::min<std::size_t>(65536,bytes.value.size()-offset);output.write(reinterpret_cast<const char*>(bytes.value.data()+offset),count);offset+=count;}output.close();if(!output)throw std::runtime_error("Image output write failed");
    }
    check(cancel);if(std::filesystem::file_size(target)>outputBudget)throw std::runtime_error("Encoded image exceeds 64 MiB");const auto decoded=decodeImageFile(target,cancel);report.outputs={report.entry};report.generation.outputHashes[report.entry.generic_u8string()]=assetTransferFingerprint(target,cancel);
    report.summary={{"domain","image"},{"width",decoded.width},{"height",decoded.height},{"precision",decoded.precision==ImagePrecision::UInt8?"uint8":decoded.precision==ImagePrecision::UInt16?"uint16":"float32"},{"colorSpace",decoded.colorSpace==ImageColorSpace::Linear?"linear":"srgb"},{"losses",report.losses},{"quality",quality}};return report;
}
nlohmann::json normalizeImageExportSettings(const std::string& format,const nlohmann::json& settings){
    if(!settings.is_object())throw std::invalid_argument("Image export settings must be an object");
    const bool qualitySupported=format=="jpeg"||format=="webp";
    for(auto it=settings.begin();it!=settings.end();++it)if(it.key()!="quality"||!qualitySupported)throw std::invalid_argument("Unknown image export setting: "+it.key());
    auto result=settings;if(qualitySupported){if(settings.contains("quality")&&!settings.at("quality").is_number_integer())throw std::invalid_argument("Image quality must be an integer");const int quality=settings.value("quality",100);if(quality<1||quality>100)throw std::invalid_argument("Image quality must be within [1,100]");result["quality"]=quality;}return result;
}
AssetTransferReport planImageExport(const AssetTransferRequest& request,const AssetCancellation& cancel){
    const auto image=decodeImageFile(request.source,cancel);AssetTransferReport report;report.summary={{"domain","image"},{"width",image.width},{"height",image.height}};
    if(request.formatId=="jpeg")report.losses.push_back("JPEG uses lossy RGB coding and omits alpha");
    if(request.formatId=="hdr")report.losses.push_back("HDR stores nonnegative RGB radiance with shared-exponent precision and omits alpha");
    if(request.formatId!="tiff"&&request.formatId!="exr"&&request.formatId!="hdr"&&!(request.formatId=="png"&&image.precision==ImagePrecision::UInt16)){
        const auto losses=convertImageToRgba8(image).losses;report.losses.insert(report.losses.begin(),losses.begin(),losses.end());}return report;
}
}
