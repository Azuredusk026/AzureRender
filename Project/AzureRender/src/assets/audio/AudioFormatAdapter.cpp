#include "assets/audio/AudioCodec.hpp"
#include "assets/AssetTransfer.hpp"
#include "assets/audio/AudioEncoder.hpp"
#include <algorithm>
#include <cstring>
namespace azurerender {
void registerAudioFormats(AssetFormatRegistry& registry){
    for(const std::string id:{"wav","mp3","flac","vorbis","opus","aac","m4a"}){
        AssetFormatDescriptor format;format.id=id;format.label=id;format.domain=AssetDomain::Audio;format.canRead=true;
        format.extensions={id=="vorbis"?".ogg":"."+id};format.features={"sample-rate","channels","loop","streaming","float32"};
        format.canWrite=id!="aac"&&id!="m4a";
        if(id=="opus")format.extensions.push_back(".ogg");
        if(format.canWrite){format.plan=planAudioExport;format.normalizeExportSettings=[id](const nlohmann::json& settings){return normalizeAudioExportSettings(id,settings);};
            format.exportSettingsSchema={{"type","object"},{"properties",{{"sampleRate",{{"type","integer"},{"minimum",0},{"maximum",192000},{"default",0}}},{"channels",{{"type","integer"},{"minimum",0},{"maximum",8},{"default",0}}}}},{"additionalProperties",false}};
            if(id=="opus"||id=="mp3"){format.exportSettingsSchema["properties"]["channels"]["maximum"]=2;format.exportSettingsSchema["properties"]["bitrate"]={{"type","integer"},{"minimum",id=="opus"?6000:8000},{"maximum",id=="opus"?512000:320000},{"default",128000}};}
            if(id=="vorbis")format.exportSettingsSchema["properties"]["quality"]={{"type","number"},{"minimum",0},{"maximum",1},{"default",.6}};}
        format.probe=[id](const std::vector<unsigned char>& bytes){
            if(id=="wav")return bytes.size()>=12&&std::memcmp(bytes.data(),"RIFF",4)==0&&std::memcmp(bytes.data()+8,"WAVE",4)==0;
            if(id=="flac")return bytes.size()>=4&&std::memcmp(bytes.data(),"fLaC",4)==0;
            if(id=="mp3")return bytes.size()>=4&&((std::memcmp(bytes.data(),"ID3",3)==0)||(bytes[0]==255&&(bytes[1]&0xE0)==0xE0&&(bytes[1]&6)!=0&&(bytes[2]&0xF0)!=0xF0));
            if(id=="aac")return bytes.size()>=7&&bytes[0]==255&&(bytes[1]&0xf6)==0xf0;
            if(id=="m4a")return bytes.size()>=12&&std::memcmp(bytes.data()+4,"ftyp",4)==0;
            const std::string prefix(bytes.begin(),bytes.end());if(id=="opus")return bytes.size()>=35&&std::memcmp(bytes.data(),"OggS",4)==0&&prefix.find("OpusHead")!=std::string::npos;return bytes.size()>=35&&std::memcmp(bytes.data(),"OggS",4)==0&&prefix.find(std::string("\1vorbis",7))!=std::string::npos;
        };
        format.prepare=[](const AssetTransferRequest& request,const AssetCancellation& cancel){
            if(request.operation==AssetTransferOperation::Export)return prepareAudioExport(request,cancel);
            auto report=prepareAssetCopy(request,cancel);const auto description=validateAudioFile(request.staging/report.entry,cancel);
            report.summary={{"domain","audio"},{"sampleRate",description.sampleRate},{"channels",description.channels},{"frames",description.frames},{"duration",description.duration},
                {"strategy",description.streaming?"stream":"predecode"},{"precision","float32"}};return report;
        };registry.add(std::move(format));
    }
}
}
