#include "assets/audio/AudioCodec.hpp"
#include "assets/AssetTransfer.hpp"
#include <algorithm>
#include <cstring>
namespace azurerender {
void registerAudioFormats(AssetFormatRegistry& registry){
    for(const std::string id:{"wav","mp3","flac","vorbis"}){
        AssetFormatDescriptor format;format.id=id;format.label=id;format.domain=AssetDomain::Audio;format.canRead=true;
        format.extensions={id=="vorbis"?".ogg":"."+id};format.features={"sample-rate","channels","loop","streaming","float32"};
        format.probe=[id](const std::vector<unsigned char>& bytes){
            if(id=="wav")return bytes.size()>=12&&std::memcmp(bytes.data(),"RIFF",4)==0&&std::memcmp(bytes.data()+8,"WAVE",4)==0;
            if(id=="flac")return bytes.size()>=4&&std::memcmp(bytes.data(),"fLaC",4)==0;
            if(id=="mp3")return bytes.size()>=4&&((std::memcmp(bytes.data(),"ID3",3)==0)||(bytes[0]==255&&(bytes[1]&0xE0)==0xE0&&(bytes[1]&6)!=0&&(bytes[2]&0xF0)!=0xF0));
            const std::string prefix(bytes.begin(),bytes.end());return bytes.size()>=35&&std::memcmp(bytes.data(),"OggS",4)==0&&prefix.find(std::string("\1vorbis",7))!=std::string::npos;
        };
        format.prepare=[](const AssetTransferRequest& request,const AssetCancellation& cancel){
            auto report=prepareAssetCopy(request,cancel);const auto description=validateAudioFile(request.staging/report.entry,cancel);
            report.summary={{"domain","audio"},{"sampleRate",description.sampleRate},{"channels",description.channels},{"frames",description.frames},{"duration",description.duration},
                {"strategy",description.streaming?"stream":"predecode"},{"precision","float32"}};return report;
        };registry.add(std::move(format));
    }
}
}
