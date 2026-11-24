#include "render/ColorGradingLut.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <limits>
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace azurerender {
namespace {
using Json=nlohmann::json;
constexpr char magic[]="AZLUT001";
constexpr std::uint32_t maximumHeaderBytes=65536;
void require(bool condition,const char* message){if(!condition)throw std::invalid_argument(message);}
void writeWord(std::ostream& stream,std::uint32_t value){
    const unsigned char bytes[]{static_cast<unsigned char>(value),static_cast<unsigned char>(value>>8),
        static_cast<unsigned char>(value>>16),static_cast<unsigned char>(value>>24)};
    stream.write(reinterpret_cast<const char*>(bytes),4);
}
std::uint32_t readWord(std::istream& stream){
    unsigned char bytes[4]{};stream.read(reinterpret_cast<char*>(bytes),4);
    if(!stream)throw std::invalid_argument("Truncated color grading LUT");
    return std::uint32_t(bytes[0])|(std::uint32_t(bytes[1])<<8)|(std::uint32_t(bytes[2])<<16)|(std::uint32_t(bytes[3])<<24);
}
float encodeSrgb(float value){return value<=.0031308F?12.92F*value:1.055F*std::pow(value,1.F/2.4F)-.055F;}
}
void validateColorGradingLut(const ColorGradingLut& lut){
    require(lut.dimension>=2&&lut.dimension<=64,"Color grading LUT dimension must be in [2,64]");
    require(lut.inputDomain==ColorGradingInputDomain::Linear||lut.inputDomain==ColorGradingInputDomain::LogC3FullLog,"Unknown LUT input domain");
    require(!lut.source.generator.empty()&&lut.source.generator.size()<=256&&lut.source.generatorVersion>0,"Color grading LUT requires generator provenance");
    require(!lut.source.inputFingerprint.empty()&&lut.source.inputFingerprint.size()<=4096,"Color grading LUT requires an input fingerprint");
    require(static_cast<unsigned>(lut.source.evidence)<=2,"Unknown LUT evidence status");
    const auto& image=lut.image;
    require(image.width==lut.dimension*lut.dimension&&image.height==lut.dimension,"Color grading LUT strip dimensions are invalid");
    require(image.precision==ImagePrecision::Float32&&image.colorSpace==ImageColorSpace::Linear,
        "Color grading LUT output must be linear float32");
    require(image.rgba32.size()==std::size_t(image.width)*image.height*4&&image.rgba8.empty()&&image.rgba16.empty()&&image.mipLevels.empty(),
        "Color grading LUT requires a single complete RGBA float32 level");
    for(const auto value:image.rgba32)require(std::isfinite(value)&&value>=0&&value<=1,"Color grading LUT samples must be finite and in [0,1]");
}
ColorGradingLut makeIdentityColorGradingLut(std::uint32_t dimension,ColorGradingInputDomain domain){
    require(dimension>=2&&dimension<=64,"Color grading LUT dimension must be in [2,64]");
    ColorGradingLut lut;lut.dimension=dimension;lut.inputDomain=domain;
    lut.source={"azure.identity-lut",1,"identity-lattice-v1",ColorGradingEvidence::Authored};
    auto& image=lut.image;image.width=dimension*dimension;image.height=dimension;image.sourceChannels=4;
    image.precision=ImagePrecision::Float32;image.colorSpace=ImageColorSpace::Linear;
    image.rgba32.resize(std::size_t(dimension)*dimension*dimension*4);
    for(unsigned g=0;g<dimension;++g)for(unsigned b=0;b<dimension;++b)for(unsigned r=0;r<dimension;++r){
        const auto offset=(std::size_t(g)*image.width+b*dimension+r)*4;
        image.rgba32[offset]=float(r)/(dimension-1);image.rgba32[offset+1]=float(g)/(dimension-1);
        image.rgba32[offset+2]=float(b)/(dimension-1);image.rgba32[offset+3]=1;
    }
    validateColorGradingLut(lut);return lut;
}
void saveColorGradingLut(const std::filesystem::path& path,const ColorGradingLut& lut){
    validateColorGradingLut(lut);
    const Json header={{"schemaVersion",ColorGradingLut::kSchemaVersion},{"dimension",lut.dimension},
        {"inputDomain",lut.inputDomain==ColorGradingInputDomain::Linear?"linear":"logc3-full-log"},
        {"outputDomain","linear"},{"layout","strip-rgb"},{"precision","float32-le"},{"channels",4},
        {"width",lut.image.width},{"height",lut.image.height},{"source",{{"generator",lut.source.generator},
            {"generatorVersion",lut.source.generatorVersion},{"inputFingerprint",lut.source.inputFingerprint},
            {"evidence",static_cast<unsigned>(lut.source.evidence)}}}};
    const auto text=header.dump();require(text.size()<=maximumHeaderBytes,"Color grading LUT header exceeds budget");
    std::ofstream stream(path,std::ios::binary|std::ios::trunc);stream.write(magic,8);writeWord(stream,static_cast<std::uint32_t>(text.size()));stream.write(text.data(),static_cast<std::streamsize>(text.size()));
    for(float value:lut.image.rgba32){std::uint32_t word;std::memcpy(&word,&value,4);writeWord(stream,word);}
    stream.flush();if(!stream)throw std::runtime_error("Cannot save color grading LUT: "+path.u8string());
}
ColorGradingLut loadColorGradingLut(const std::filesystem::path& path){
    std::ifstream stream(path,std::ios::binary|std::ios::ate);
    if(!stream)throw std::runtime_error("Cannot load color grading LUT: "+path.u8string());
    const auto length=static_cast<std::streamoff>(stream.tellg());require(length>=12&&length<=12+maximumHeaderBytes+64*64*64*16,"Color grading LUT byte size exceeds format limits");stream.seekg(0);
    char signature[8]{};stream.read(signature,8);require(std::memcmp(signature,magic,8)==0,"Invalid color grading LUT signature");
    const auto headerBytes=readWord(stream);require(headerBytes>0&&headerBytes<=maximumHeaderBytes&&std::streamoff(headerBytes)<=length-12,"Invalid color grading LUT header size");
    std::string text(headerBytes,'\0');stream.read(text.data(),headerBytes);const auto header=Json::parse(text);
    require(header.at("schemaVersion").is_number_unsigned()||header.at("schemaVersion").is_number_integer(),"Invalid color grading LUT schema");
    require(header.at("schemaVersion")==1,"Unsupported color grading LUT schema");
    require(header.at("dimension").is_number_integer()&&header.at("dimension")>=2&&header.at("dimension")<=64,
        "Color grading LUT dimension must be an integer in [2,64]");
    const auto n=header.at("dimension").get<int>();
    require(header.at("outputDomain")=="linear"&&header.at("layout")=="strip-rgb"&&header.at("precision")=="float32-le"&&header.at("channels")==4,
        "Unsupported color grading LUT storage contract");
    ColorGradingLut lut;lut.dimension=static_cast<unsigned>(n);const auto domain=header.at("inputDomain").get<std::string>();
    require(domain=="linear"||domain=="logc3-full-log","Unknown LUT input domain");
    lut.inputDomain=domain=="linear"?ColorGradingInputDomain::Linear:ColorGradingInputDomain::LogC3FullLog;
    const auto& source=header.at("source");
    require(source.at("generatorVersion").is_number_integer()&&source.at("generatorVersion")>0
        &&source.at("generatorVersion")<=std::numeric_limits<std::uint32_t>::max(),"Invalid LUT generator version");
    require(source.at("evidence").is_number_integer()&&source.at("evidence")>=0&&source.at("evidence")<=2,"Unknown LUT evidence status");
    lut.source={source.at("generator").get<std::string>(),source.at("generatorVersion").get<unsigned>(),
        source.at("inputFingerprint").get<std::string>(),static_cast<ColorGradingEvidence>(source.at("evidence").get<unsigned>())};
    require(header.at("width").is_number_integer()&&header.at("height").is_number_integer()
        &&header.at("width")==n*n&&header.at("height")==n,"Color grading LUT strip dimensions are invalid");
    auto& image=lut.image;image.width=header.at("width").get<unsigned>();image.height=header.at("height").get<unsigned>();
    require(image.width==lut.dimension*lut.dimension&&image.height==lut.dimension,"Color grading LUT strip dimensions are invalid");
    const std::size_t count=std::size_t(n)*n*n*4;
    require(length==std::streamoff(12+headerBytes+count*4),"Color grading LUT pixel byte count is invalid");
    image.precision=ImagePrecision::Float32;image.colorSpace=ImageColorSpace::Linear;image.sourceChannels=4;image.rgba32.resize(count);
    for(auto& value:image.rgba32){const auto word=readWord(stream);std::memcpy(&value,&word,4);}
    validateColorGradingLut(lut);return lut;
}
float colorGradingInputCoordinate(float value,ColorGradingInputDomain domain){
    require(std::isfinite(value),"Color grading input must be finite");
    if(domain==ColorGradingInputDomain::Linear)return std::clamp(value,0.F,1.F);
    require(domain==ColorGradingInputDomain::LogC3FullLog,"Unknown LUT input domain");
    // Full logarithmic branch, without the piecewise LogC toe. log(0) maps to the lower edge.
    const float argument=std::max(value*5.555555820465088F+.047995999455451965F,0.F);
    if(argument<=0)return 0;
    return std::clamp(std::log2(argument)*.3010300099849701F*.244160994887352F+.3860360085964203F,0.F,1.F);
}
std::array<float,3> sampleColorGradingLut(const ColorGradingLut& lut,const std::array<float,3>& sceneLinear,
    float exposureMultiplier,ColorGradingOutputEncoding encoding){
    validateColorGradingLut(lut);
    require(std::isfinite(exposureMultiplier)&&exposureMultiplier>0&&exposureMultiplier<=65536,"LUT exposure multiplier must be in (0,65536]");
    require(encoding==ColorGradingOutputEncoding::Linear||encoding==ColorGradingOutputEncoding::Srgb,"Unknown LUT output encoding");
    std::array<unsigned,3> lower{},upper{};std::array<float,3> fraction{};
    for(unsigned axis=0;axis<3;++axis){
        require(std::isfinite(sceneLinear[axis])&&std::isfinite(sceneLinear[axis]*exposureMultiplier),"Color grading exposed input must be finite");
        const auto coordinate=colorGradingInputCoordinate(sceneLinear[axis]*exposureMultiplier,lut.inputDomain)*(lut.dimension-1);
        lower[axis]=static_cast<unsigned>(std::floor(coordinate));upper[axis]=std::min(lower[axis]+1,lut.dimension-1);fraction[axis]=coordinate-lower[axis];
    }
    std::array<float,3> result{};
    for(unsigned corner=0;corner<8;++corner){
        std::array<unsigned,3> cell{};float weight=1;
        for(unsigned axis=0;axis<3;++axis){const bool high=(corner&(1U<<axis))!=0;cell[axis]=high?upper[axis]:lower[axis];weight*=high?fraction[axis]:1-fraction[axis];}
        const auto offset=(std::size_t(cell[1])*lut.image.width+cell[2]*lut.dimension+cell[0])*4;
        for(unsigned channel=0;channel<3;++channel)result[channel]+=lut.image.rgba32[offset+channel]*weight;
    }
    if(encoding==ColorGradingOutputEncoding::Srgb)for(auto& value:result)value=encodeSrgb(value);
    return result;
}
}
