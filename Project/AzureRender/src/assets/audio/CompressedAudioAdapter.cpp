#include "assets/audio/CompressedAudioAdapter.hpp"
#include <opus/opusfile.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wrl/client.h>
#endif
namespace azurerender {namespace {
void check(const AssetCancellation& cancel){if(cancel&&cancel())throw std::runtime_error("Compressed audio operation cancelled");}
void validate(AudioDescription& value){if(value.channels<1||value.channels>8||value.sampleRate<8000||value.sampleRate>192000||!value.frames||value.frames>std::uint64_t(value.sampleRate)*7200)throw std::invalid_argument("Compressed audio rate, layout or duration exceeds its budget");value.duration=double(value.frames)/value.sampleRate;value.streaming=value.duration>30;}
std::vector<unsigned char> sourceBytes(const std::filesystem::path& path,const AssetCancellation& cancel){check(cancel);std::ifstream input(path,std::ios::binary|std::ios::ate);const auto size=input.tellg();if(size<12||size>128LL*1024*1024)throw std::invalid_argument("Compressed audio source exceeds 128 MiB or is unavailable");std::vector<unsigned char> result(static_cast<std::size_t>(size));input.seekg(0);for(std::size_t at=0;at<result.size();){check(cancel);const auto count=std::min<std::size_t>(65536,result.size()-at);input.read(reinterpret_cast<char*>(result.data()+at),count);if(!input)throw std::runtime_error("Truncated compressed audio source");at+=count;}return result;}
void checkOpusPages(std::vector<unsigned char>& bytes,const AssetCancellation& cancel){
    bool end=false;for(std::size_t at=0;at<bytes.size();){check(cancel);if(bytes.size()-at<27||std::memcmp(bytes.data()+at,"OggS",4)!=0||bytes[at+4]!=0)throw std::runtime_error("Invalid or truncated Opus page header");const auto header=27U+bytes[at+26];if(header>bytes.size()-at)throw std::runtime_error("Truncated Opus segment table");unsigned body=0;for(unsigned i=27;i<header;++i)body+=bytes[at+i];if(body>bytes.size()-at-header)throw std::runtime_error("Truncated Opus page payload");std::array<unsigned char,4> checksum{};std::copy_n(bytes.data()+at+22,4,checksum.begin());ogg_page page{bytes.data()+at,static_cast<long>(header),bytes.data()+at+header,static_cast<long>(body)};ogg_page_checksum_set(&page);if(!std::equal(checksum.begin(),checksum.end(),bytes.data()+at+22))throw std::runtime_error("Opus page checksum is invalid");end=(bytes[at+5]&4)!=0;at+=header+body;}if(!end)throw std::runtime_error("Opus stream is missing its end page");
}
class OpusReader final:public AudioFrameReader {
    std::vector<unsigned char> bytes_;std::unique_ptr<OggOpusFile,decltype(&op_free)> file_{nullptr,op_free};AudioDescription description_;
public:
    OpusReader(const std::filesystem::path& path,const AssetCancellation& cancel):bytes_(sourceBytes(path,cancel)){
        checkOpusPages(bytes_,cancel);int error=0;file_.reset(op_open_memory(bytes_.data(),bytes_.size(),&error));if(!file_||op_link_count(file_.get())!=1)throw std::runtime_error("Invalid Opus container or unsupported chained stream");
        const auto frames=op_pcm_total(file_.get(),-1);if(frames<=0)throw std::runtime_error("Opus granule length is invalid");description_.sampleRate=48000;description_.channels=static_cast<unsigned>(op_channel_count(file_.get(),-1));description_.frames=static_cast<std::uint64_t>(frames);validate(description_);check(cancel);
    }
    AudioDescription description()const override{return description_;}
    std::uint64_t read(float* samples,std::uint64_t frames,const AssetCancellation& cancel)override{check(cancel);const auto count=static_cast<int>(std::min<std::uint64_t>(frames,4096)*description_.channels);const int got=op_read_float(file_.get(),samples,count,nullptr);if(got<0)throw std::runtime_error("Opus sample decode failed");for(std::size_t i=0;i<std::size_t(got)*description_.channels;++i)if(!std::isfinite(samples[i]))throw std::runtime_error("Non-finite Opus samples");check(cancel);return static_cast<std::uint64_t>(got);}
    void seek(std::uint64_t frame,const AssetCancellation& cancel)override{check(cancel);if(frame>description_.frames||op_pcm_seek(file_.get(),static_cast<ogg_int64_t>(frame))!=0)throw std::runtime_error("Opus seek is outside its valid PCM range");}
};
void checkNativeContainer(const std::vector<unsigned char>& bytes){
    if(bytes[0]==255&&(bytes[1]&0xf6)==0xf0){for(std::size_t at=0;at<bytes.size();){if(bytes.size()-at<7||bytes[at]!=255||(bytes[at+1]&0xf6)!=0xf0)throw std::runtime_error("Invalid ADTS packet header");const auto count=(unsigned(bytes[at+3]&3)<<11)|(unsigned(bytes[at+4])<<3)|(bytes[at+5]>>5);const unsigned header=(bytes[at+1]&1)?7:9;if(count<header||count>bytes.size()-at)throw std::runtime_error("Truncated ADTS packet");at+=count;}return;}
    const auto big=[&](std::size_t at){if(at+4>bytes.size())throw std::runtime_error("Truncated M4A atom");return (std::uint64_t(bytes[at])<<24)|(std::uint64_t(bytes[at+1])<<16)|(std::uint64_t(bytes[at+2])<<8)|bytes[at+3];};
    bool media=false,metadata=false;for(std::size_t at=0;at<bytes.size();){if(bytes.size()-at<8)throw std::runtime_error("Truncated M4A atom");auto size=big(at);std::uint64_t header=8;if(size==1){size=(big(at+8)<<32)|big(at+12);header=16;}else if(size==0)size=bytes.size()-at;if(size<header||size>bytes.size()-at)throw std::runtime_error("M4A atom exceeds its container");media=media||std::memcmp(bytes.data()+at+4,"mdat",4)==0;metadata=metadata||std::memcmp(bytes.data()+at+4,"moov",4)==0;at+=static_cast<std::size_t>(size);}if(!media||!metadata)throw std::runtime_error("M4A requires audio media and track metadata");
}
#ifdef _WIN32
using Microsoft::WRL::ComPtr;
void mf(HRESULT status){if(FAILED(status))throw std::runtime_error("Windows audio decoder failed: "+std::to_string(static_cast<unsigned>(status)));}
class MfReader final:public AudioFrameReader {
    std::filesystem::path path_;ComPtr<IMFSourceReader> reader_;AudioDescription description_;std::vector<float> pending_;std::size_t offset_=0;std::uint64_t cursor_=0;bool end_=false,com_=false,started_=false;
    void open(){reader_.Reset();pending_.clear();offset_=0;end_=false;cursor_=0;mf(MFCreateSourceReaderFromURL(path_.c_str(),nullptr,&reader_));mf(reader_->SetStreamSelection(static_cast<DWORD>(MF_SOURCE_READER_ALL_STREAMS),FALSE));mf(reader_->SetStreamSelection(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM),TRUE));ComPtr<IMFMediaType> type;mf(MFCreateMediaType(&type));mf(type->SetGUID(MF_MT_MAJOR_TYPE,MFMediaType_Audio));mf(type->SetGUID(MF_MT_SUBTYPE,MFAudioFormat_Float));mf(reader_->SetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM),nullptr,type.Get()));mf(reader_->GetCurrentMediaType(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM),&type));UINT32 channels=0,rate=0;mf(type->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS,&channels));mf(type->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND,&rate));if(description_.channels&&(channels!=description_.channels||rate!=description_.sampleRate))throw std::runtime_error("Native audio layout changed while reopening");description_.channels=channels;description_.sampleRate=rate;if(channels<1||channels>8||rate<8000||rate>192000)throw std::runtime_error("Unsupported native audio layout");}
    void shutdown(){reader_.Reset();if(started_)MFShutdown();if(com_)CoUninitialize();started_=com_=false;}
public:
    MfReader(const std::filesystem::path& path,const AssetCancellation& cancel):path_(std::filesystem::absolute(path)){
        const auto bytes=sourceBytes(path,cancel);checkNativeContainer(bytes);
        try{const auto status=CoInitializeEx(nullptr,COINIT_MULTITHREADED);if(status!=RPC_E_CHANGED_MODE){mf(status);com_=true;}mf(MFStartup(MF_VERSION));started_=true;open();std::vector<float> block(4096*description_.channels);std::uint64_t frames=0;for(;;){const auto got=read(block.data(),4096,cancel);if(!got)break;frames+=got;if(frames>std::uint64_t(description_.sampleRate)*7200)throw std::runtime_error("Native audio exceeds two hours");}description_.frames=frames;validate(description_);open();}catch(...){shutdown();throw;}
    }
    ~MfReader()override{shutdown();}
    AudioDescription description()const override{return description_;}
    std::uint64_t read(float* samples,std::uint64_t frames,const AssetCancellation& cancel)override{
        check(cancel);frames=std::min<std::uint64_t>(frames,4096);std::uint64_t got=0;
        while(got<frames){if(offset_==pending_.size()){pending_.clear();offset_=0;if(end_)break;DWORD flags=0;ComPtr<IMFSample> sample;mf(reader_->ReadSample(static_cast<DWORD>(MF_SOURCE_READER_FIRST_AUDIO_STREAM),0,nullptr,&flags,nullptr,&sample));if(flags&(MF_SOURCE_READERF_ERROR|MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED))throw std::runtime_error("Native audio decode error or layout change");end_=(flags&MF_SOURCE_READERF_ENDOFSTREAM)!=0;if(!sample){check(cancel);continue;}ComPtr<IMFMediaBuffer> buffer;mf(sample->ConvertToContiguousBuffer(&buffer));DWORD length=0;mf(buffer->GetCurrentLength(&length));if(length>1024*1024||length%(sizeof(float)*description_.channels))throw std::runtime_error("Native audio block exceeds its budget or has an invalid layout");pending_.resize(length/sizeof(float));BYTE* data=nullptr;mf(buffer->Lock(&data,nullptr,nullptr));std::memcpy(pending_.data(),data,length);mf(buffer->Unlock());for(float value:pending_)if(!std::isfinite(value))throw std::runtime_error("Non-finite native audio samples");if(pending_.empty()){check(cancel);continue;}}
            const auto count=std::min<std::uint64_t>(frames-got,(pending_.size()-offset_)/description_.channels);std::copy_n(pending_.data()+offset_,static_cast<std::size_t>(count)*description_.channels,samples+got*description_.channels);offset_+=static_cast<std::size_t>(count)*description_.channels;got+=count;check(cancel);
        }cursor_+=got;return got;
    }
    void seek(std::uint64_t frame,const AssetCancellation& cancel)override{check(cancel);if(frame>description_.frames)throw std::runtime_error("Native audio seek exceeds its PCM range");open();std::vector<float> block(4096*description_.channels);while(cursor_<frame){const auto got=read(block.data(),std::min<std::uint64_t>(4096,frame-cursor_),cancel);if(!got)throw std::runtime_error("Native audio seek reached unexpected end");}}
};
#endif
}
std::unique_ptr<AudioFrameReader> openCompressedAudioReader(const std::filesystem::path& path,const AssetCancellation& cancel){
    check(cancel);std::ifstream input(path,std::ios::binary);std::vector<unsigned char> prefix(4096);input.read(reinterpret_cast<char*>(prefix.data()),prefix.size());prefix.resize(static_cast<std::size_t>(input.gcount()));
    constexpr char opusMagic[]="OpusHead";
    if(prefix.size()>=35&&std::memcmp(prefix.data(),"OggS",4)==0&&std::search(prefix.begin(),prefix.end(),opusMagic,opusMagic+8)!=prefix.end())return std::make_unique<OpusReader>(path,cancel);
    const bool aac=prefix.size()>=7&&prefix[0]==255&&(prefix[1]&0xf6)==0xf0;
    const bool m4a=prefix.size()>=12&&std::memcmp(prefix.data()+4,"ftyp",4)==0;
    if(aac||m4a){
#ifdef _WIN32
        return std::make_unique<MfReader>(path,cancel);
#else
        throw std::runtime_error("AAC and M4A require the Windows media decoder");
#endif
    }return {};
}
}
