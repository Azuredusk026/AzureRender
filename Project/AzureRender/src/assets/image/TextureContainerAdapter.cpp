#include "assets/image/TextureContainerAdapter.hpp"
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <DirectXTex.h>
#include <ktx.h>
#include <vulkan/vulkan_core.h>
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <memory>
#include <stdexcept>
namespace azurerender {namespace {
constexpr std::size_t byteBudget=64ULL*1024*1024,pixelBudget=256ULL*1024*1024;
constexpr unsigned char ktxMagic[]={0xab,0x4b,0x54,0x58,0x20,0x32,0x30,0xbb,0x0d,0x0a,0x1a,0x0a};
void check(const AssetCancellation& cancel){if(cancel&&cancel())throw std::runtime_error("Texture container operation cancelled");}
void dx(HRESULT value){if(FAILED(value))throw std::runtime_error("DDS pixel layout, data or conversion is invalid");}
void kt(KTX_error_code value){if(value!=KTX_SUCCESS)throw std::runtime_error(std::string("KTX2 operation failed: ")+ktxErrorString(value));}
void extent(std::size_t width,std::size_t height,std::size_t levels){
    if(!width||!height||width>8192||height>8192||!levels)throw std::invalid_argument("Texture dimensions exceed 8192 or mip count is invalid");
    unsigned maximum=1;for(auto side=std::max(width,height);side>1;side/=2)++maximum;
    if(levels>maximum)throw std::invalid_argument("Texture mip count exceeds its extent");
    std::uint64_t total=0;for(std::size_t i=0;i<levels;++i){total+=width*height*4;if(total>pixelBudget)throw std::invalid_argument("Decoded texture mip chain exceeds 256 MiB");width=std::max(width/2,std::size_t(1));height=std::max(height/2,std::size_t(1));}
}
ImageMipLevel pixels(const DirectX::Image& source,const AssetCancellation& cancel){
    check(cancel);DirectX::ScratchImage converted;const DirectX::Image* image=&source;
    // Preserve encoded color bytes. The material's GPU view owns sRGB decoding.
    const auto target=DirectX::IsSRGB(source.format)?DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:DXGI_FORMAT_R8G8B8A8_UNORM;
    if(DirectX::IsCompressed(source.format)){dx(DirectX::Decompress(source,target,converted));image=converted.GetImage(0,0,0);}
    else if(source.format!=target){dx(DirectX::Convert(source,target,DirectX::TEX_FILTER_DEFAULT,.5F,converted));image=converted.GetImage(0,0,0);}
    ImageMipLevel result{static_cast<unsigned>(image->width),static_cast<unsigned>(image->height),{}};result.rgba8.resize(image->width*image->height*4);
    for(std::size_t y=0;y<image->height;++y){check(cancel);std::memcpy(result.rgba8.data()+y*image->width*4,image->pixels+y*image->rowPitch,image->width*4);}return result;
}
std::string compressionName(DXGI_FORMAT format){switch(format){case DXGI_FORMAT_BC1_UNORM:case DXGI_FORMAT_BC1_UNORM_SRGB:return "bc1";case DXGI_FORMAT_BC2_UNORM:case DXGI_FORMAT_BC2_UNORM_SRGB:return "bc2";case DXGI_FORMAT_BC3_UNORM:case DXGI_FORMAT_BC3_UNORM_SRGB:return "bc3";case DXGI_FORMAT_BC4_UNORM:return "bc4";case DXGI_FORMAT_BC5_UNORM:return "bc5";case DXGI_FORMAT_BC7_UNORM:case DXGI_FORMAT_BC7_UNORM_SRGB:return "bc7";default:return DirectX::IsCompressed(format)?"bc":"rgba";}}
unsigned u32(const std::vector<unsigned char>& b,std::size_t at){if(at+4>b.size())throw std::invalid_argument("Truncated texture header");unsigned value=0;for(unsigned i=0;i<4;++i)value|=unsigned(b[at+i])<<(8*i);return value;}
std::uint64_t u64(const std::vector<unsigned char>& b,std::size_t at){return std::uint64_t(u32(b,at))|(std::uint64_t(u32(b,at+4))<<32);}
DXGI_FORMAT dxFormat(unsigned format){switch(format){
case VK_FORMAT_R8G8B8A8_UNORM:return DXGI_FORMAT_R8G8B8A8_UNORM;
case VK_FORMAT_R8G8B8A8_SRGB:return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
case VK_FORMAT_BC1_RGB_UNORM_BLOCK:case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:return DXGI_FORMAT_BC1_UNORM;
case VK_FORMAT_BC1_RGB_SRGB_BLOCK:case VK_FORMAT_BC1_RGBA_SRGB_BLOCK:return DXGI_FORMAT_BC1_UNORM_SRGB;
case VK_FORMAT_BC2_UNORM_BLOCK:return DXGI_FORMAT_BC2_UNORM;
case VK_FORMAT_BC2_SRGB_BLOCK:return DXGI_FORMAT_BC2_UNORM_SRGB;
case VK_FORMAT_BC3_UNORM_BLOCK:return DXGI_FORMAT_BC3_UNORM;
case VK_FORMAT_BC3_SRGB_BLOCK:return DXGI_FORMAT_BC3_UNORM_SRGB;
case VK_FORMAT_BC4_UNORM_BLOCK:return DXGI_FORMAT_BC4_UNORM;
case VK_FORMAT_BC5_UNORM_BLOCK:return DXGI_FORMAT_BC5_UNORM;
case VK_FORMAT_BC7_UNORM_BLOCK:return DXGI_FORMAT_BC7_UNORM;
case VK_FORMAT_BC7_SRGB_BLOCK:return DXGI_FORMAT_BC7_UNORM_SRGB;
default:throw std::invalid_argument("Unsupported KTX2 pixel format");}}
using Ktx=std::unique_ptr<ktxTexture2,void(*)(ktxTexture2*)>;
void destroy(ktxTexture2* t){ktxTexture_Destroy(ktxTexture(t));}
DecodedImage finish(DecodedImage result){result.width=result.mipLevels.front().width;result.height=result.mipLevels.front().height;result.sourceChannels=4;result.rgba8=result.mipLevels.front().rgba8;return result;}
}
bool isTextureContainer(const std::vector<unsigned char>& b){return (b.size()>=4&&std::memcmp(b.data(),"DDS ",4)==0)||(b.size()>=12&&std::memcmp(b.data(),ktxMagic,12)==0);}
DecodedImage decodeTextureContainer(const std::vector<unsigned char>& bytes,const AssetCancellation& cancel){
    check(cancel);if(bytes.size()>byteBudget||!isTextureContainer(bytes))throw std::invalid_argument("Invalid or oversized texture container");DecodedImage result;
    if(std::memcmp(bytes.data(),"DDS ",4)==0){
        DirectX::TexMetadata metadata{};dx(DirectX::GetMetadataFromDDSMemory(bytes.data(),bytes.size(),DirectX::DDS_FLAGS_NONE,metadata));
        if(metadata.dimension!=DirectX::TEX_DIMENSION_TEXTURE2D||metadata.arraySize!=1||metadata.IsCubemap())throw std::invalid_argument("DDS supports a single two-dimensional texture");
        extent(metadata.width,metadata.height,metadata.mipLevels);
        std::size_t storage=0,width=metadata.width,height=metadata.height;
        for(std::size_t level=0;level<metadata.mipLevels;++level){
            std::size_t row=0,slice=0;dx(DirectX::ComputePitch(metadata.format,width,height,row,slice));
            if(slice>byteBudget-storage)throw std::invalid_argument("DDS source mip storage exceeds 64 MiB");
            storage+=slice;width=std::max(width/2,std::size_t(1));height=std::max(height/2,std::size_t(1));
        }
        DirectX::ScratchImage source;dx(DirectX::LoadFromDDSMemory(bytes.data(),bytes.size(),DirectX::DDS_FLAGS_NONE,nullptr,source));
        result.colorSpace=DirectX::IsSRGB(metadata.format)?ImageColorSpace::Srgb:ImageColorSpace::Linear;result.sourceCompression=compressionName(metadata.format);
        for(std::size_t level=0;level<metadata.mipLevels;++level)result.mipLevels.push_back(pixels(*source.GetImage(level,0,0),cancel));
    }else{
        // Validate dimensional budgets before the library allocates level data.
        const auto w=u32(bytes,20),h=u32(bytes,24),depth=u32(bytes,28),layers=u32(bytes,32),faces=u32(bytes,36),levels=u32(bytes,40);
        if(!h||depth||layers>1||faces!=1)throw std::invalid_argument("KTX2 supports a single two-dimensional texture");extent(w,h,levels);
        const auto indexEnd=80ULL+24ULL*levels;
        const auto range=[&](std::uint64_t offset,std::uint64_t length){if(length&&(offset<indexEnd||offset>bytes.size()||length>bytes.size()-offset))throw std::invalid_argument("KTX2 metadata or mip range is invalid");};
        std::uint64_t declared=0;
        for(unsigned level=0;level<levels;++level){const auto at=80+std::size_t(level)*24;const auto size=u64(bytes,at+16);
            if(size>pixelBudget-declared)throw std::invalid_argument("KTX2 declared mip storage exceeds 256 MiB");declared+=size;
            range(u64(bytes,at),u64(bytes,at+8));}
        range(u32(bytes,48),u32(bytes,52));range(u32(bytes,56),u32(bytes,60));range(u64(bytes,64),u64(bytes,72));
        ktxTexture2* raw=nullptr;kt(ktxTexture2_CreateFromMemory(bytes.data(),bytes.size(),KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,&raw));Ktx texture(raw,destroy);
        if(texture->isArray||texture->isCubemap||texture->numDimensions!=2)throw std::invalid_argument("KTX2 arrays, cubes and volumes require separate support");
        result.colorSpace=ktxTexture2_GetOETF(texture.get())==KHR_DF_TRANSFER_SRGB?ImageColorSpace::Srgb:ImageColorSpace::Linear;
        const bool basis=ktxTexture2_NeedsTranscoding(texture.get());result.sourceCompression=basis?"basis":compressionName(dxFormat(texture->vkFormat));
        if(basis){check(cancel);kt(ktxTexture2_TranscodeBasis(texture.get(),KTX_TTF_RGBA32,0));check(cancel);}
        const auto format=dxFormat(texture->vkFormat);unsigned width=w,height=h;
        for(unsigned level=0;level<levels;++level){ktx_size_t offset=0;kt(ktxTexture_GetImageOffset(ktxTexture(texture.get()),level,0,0,&offset));
            std::size_t row=0,slice=0;dx(DirectX::ComputePitch(format,width,height,row,slice));if(offset>texture->dataSize||slice>texture->dataSize-offset)throw std::runtime_error("Truncated KTX2 mip data");
            DirectX::Image image{width,height,format,row,slice,texture->pData+offset};auto decoded=pixels(image,cancel);
            if(texture->orientation.x==KTX_ORIENT_X_LEFT||texture->orientation.y==KTX_ORIENT_Y_UP){auto original=decoded.rgba8;for(unsigned y=0;y<height;++y){check(cancel);for(unsigned x=0;x<width;++x){const unsigned sx=texture->orientation.x==KTX_ORIENT_X_LEFT?width-1-x:x,sy=texture->orientation.y==KTX_ORIENT_Y_UP?height-1-y:y;std::memcpy(decoded.rgba8.data()+(std::size_t(y)*width+x)*4,original.data()+(std::size_t(sy)*width+sx)*4,4);}}}
            result.mipLevels.push_back(std::move(decoded));width=std::max(width/2,1U);height=std::max(height/2,1U);}
    }check(cancel);return finish(std::move(result));
}
std::vector<unsigned char> encodeTextureContainer(const DecodedImage& image,const std::string& format,const nlohmann::json& settings,const AssetCancellation& cancel){
    check(cancel);auto levels=image.mipLevels;if(levels.empty())levels.push_back({image.width,image.height,convertImageToRgba8(image,false).pixels});extent(image.width,image.height,levels.size());
    unsigned w=image.width,h=image.height;for(const auto& level:levels){if(level.width!=w||level.height!=h||level.rgba8.size()!=std::size_t(w)*h*4)throw std::invalid_argument("Invalid authored RGBA mip chain");w=std::max(w/2,1U);h=std::max(h/2,1U);}
    const auto compression=settings.value("compression",std::string("rgba"));std::vector<unsigned char> output;
    const bool srgb=image.colorSpace==ImageColorSpace::Srgb;
    if(format=="dds"){
        std::vector<DirectX::Image> views;for(auto& level:levels)views.push_back({level.width,level.height,srgb?DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:DXGI_FORMAT_R8G8B8A8_UNORM,std::size_t(level.width)*4,level.rgba8.size(),level.rgba8.data()});
        DirectX::TexMetadata metadata{};metadata.width=image.width;metadata.height=image.height;metadata.depth=1;metadata.arraySize=1;metadata.mipLevels=levels.size();metadata.dimension=DirectX::TEX_DIMENSION_TEXTURE2D;metadata.format=views.front().format;
        DirectX::ScratchImage compressed;const DirectX::Image* data=views.data();std::size_t count=views.size();
        if(compression!="rgba"){DXGI_FORMAT target;
            if(compression=="bc1")target=srgb?DXGI_FORMAT_BC1_UNORM_SRGB:DXGI_FORMAT_BC1_UNORM;
            else if(compression=="bc3")target=srgb?DXGI_FORMAT_BC3_UNORM_SRGB:DXGI_FORMAT_BC3_UNORM;
            else if(compression=="bc5")target=DXGI_FORMAT_BC5_UNORM;
            else if(compression=="bc7")target=srgb?DXGI_FORMAT_BC7_UNORM_SRGB:DXGI_FORMAT_BC7_UNORM;
            else throw std::invalid_argument("Unsupported DDS compression");
            DirectX::CompressOptions options{};options.flags=DirectX::TEX_COMPRESS_DEFAULT;options.threshold=.5F;options.alphaWeight=1;
            dx(DirectX::CompressEx(views.data(),views.size(),metadata,target,options,compressed,[&](std::size_t,std::size_t){return !cancel||!cancel();}));check(cancel);data=compressed.GetImages();count=compressed.GetImageCount();metadata=compressed.GetMetadata();}
        DirectX::Blob blob;dx(DirectX::SaveToDDSMemory(data,count,metadata,DirectX::DDS_FLAGS_FORCE_DX10_EXT,blob));if(blob.GetBufferSize()>byteBudget)throw std::invalid_argument("DDS output exceeds 64 MiB");output.assign(blob.GetBufferPointer(),blob.GetBufferPointer()+blob.GetBufferSize());
    }else if(format=="ktx2"){
        ktxTextureCreateInfo info{};info.vkFormat=srgb?VK_FORMAT_R8G8B8A8_SRGB:VK_FORMAT_R8G8B8A8_UNORM;info.baseWidth=image.width;info.baseHeight=image.height;info.baseDepth=1;info.numDimensions=2;info.numLevels=static_cast<unsigned>(levels.size());info.numLayers=1;info.numFaces=1;
        ktxTexture2* raw=nullptr;kt(ktxTexture2_Create(&info,KTX_TEXTURE_CREATE_ALLOC_STORAGE,&raw));Ktx texture(raw,destroy);
        for(unsigned level=0;level<levels.size();++level){check(cancel);kt(ktxTexture_SetImageFromMemory(ktxTexture(texture.get()),level,0,0,levels[level].rgba8.data(),levels[level].rgba8.size()));}
        if(compression=="basis"){check(cancel);ktxBasisParams params{};params.structSize=sizeof(params);params.uastc=KTX_TRUE;params.threadCount=4;params.uastcFlags=KTX_PACK_UASTC_LEVEL_DEFAULT;kt(ktxTexture2_CompressBasisEx(texture.get(),&params));check(cancel);}else if(compression!="rgba")throw std::invalid_argument("Unsupported KTX2 compression");
        ktx_uint8_t* buffer=nullptr;ktx_size_t size=0;kt(ktxTexture_WriteToMemory(ktxTexture(texture.get()),&buffer,&size));std::unique_ptr<ktx_uint8_t,decltype(&std::free)> owned(buffer,std::free);if(size>byteBudget)throw std::invalid_argument("KTX2 output exceeds 64 MiB");output.assign(buffer,buffer+size);
    }else throw std::invalid_argument("Unknown texture container output");check(cancel);return output;
}
}
