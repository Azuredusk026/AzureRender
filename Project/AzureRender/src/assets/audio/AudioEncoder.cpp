#include "assets/audio/AudioEncoder.hpp"
#include "assets/audio/AudioCodec.hpp"
#include <FLAC/stream_encoder.h>
#include <vorbis/vorbisenc.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <memory>
#include <stdexcept>
namespace azurerender {namespace {
constexpr std::uint64_t outputBudget=128ULL*1024*1024;
void check(const AssetCancellation& cancel){if(cancel&&cancel())throw std::runtime_error("Audio export cancelled");}
struct Output{
    std::ofstream file;AssetCancellation cancel;std::uint64_t bytes=0;
    Output(const std::filesystem::path& path,AssetCancellation cancellation):file(path,std::ios::binary),cancel(std::move(cancellation)){if(!file)throw std::runtime_error("Audio output unavailable");}
    bool write(const void* data,std::size_t count){if((cancel&&cancel())||count>outputBudget-bytes)return false;file.write(static_cast<const char*>(data),count);bytes+=count;return static_cast<bool>(file);}
    void finish(){check(cancel);file.close();if(!file)throw std::runtime_error("Audio output write failed");}
};
void le(Output& output,std::uint32_t value,unsigned count){unsigned char bytes[4];for(unsigned i=0;i<count;++i)bytes[i]=static_cast<unsigned char>(value>>(8*i));if(!output.write(bytes,count))throw std::runtime_error("Audio write cancelled or failed");}
void wave(Output& output,const DecodedAudio& audio){
    const auto count=static_cast<std::uint64_t>(audio.samples.size())*4;if(count+44>outputBudget)throw std::runtime_error("WAV output exceeds 128 MiB");
    if(!output.write("RIFF",4))throw std::runtime_error("WAV header write failed");le(output,static_cast<std::uint32_t>(36+count),4);if(!output.write("WAVEfmt ",8))throw std::runtime_error("WAV header write failed");le(output,16,4);le(output,3,2);le(output,audio.description.channels,2);le(output,audio.description.sampleRate,4);le(output,audio.description.sampleRate*audio.description.channels*4,4);le(output,audio.description.channels*4,2);le(output,32,2);if(!output.write("data",4))throw std::runtime_error("WAV data header write failed");le(output,static_cast<std::uint32_t>(count),4);
    for(float sample:audio.samples){std::uint32_t bits;std::memcpy(&bits,&sample,4);le(output,bits,4);}
}
FLAC__StreamEncoderWriteStatus flacWrite(const FLAC__StreamEncoder*,const FLAC__byte bytes[],size_t count,unsigned,unsigned,void* client){return static_cast<Output*>(client)->write(bytes,count)?FLAC__STREAM_ENCODER_WRITE_STATUS_OK:FLAC__STREAM_ENCODER_WRITE_STATUS_FATAL_ERROR;}
void flac(Output& output,const DecodedAudio& audio){
    std::unique_ptr<FLAC__StreamEncoder,decltype(&FLAC__stream_encoder_delete)> encoder(FLAC__stream_encoder_new(),FLAC__stream_encoder_delete);if(!encoder)throw std::runtime_error("FLAC encoder allocation failed");
    if(!FLAC__stream_encoder_set_channels(encoder.get(),audio.description.channels)||!FLAC__stream_encoder_set_sample_rate(encoder.get(),audio.description.sampleRate)||!FLAC__stream_encoder_set_bits_per_sample(encoder.get(),24)||!FLAC__stream_encoder_set_total_samples_estimate(encoder.get(),audio.description.frames)||FLAC__stream_encoder_init_stream(encoder.get(),flacWrite,nullptr,nullptr,nullptr,&output)!=FLAC__STREAM_ENCODER_INIT_STATUS_OK)throw std::runtime_error("FLAC encoder configuration failed");
    std::vector<FLAC__int32> block(4096*audio.description.channels);
    for(std::uint64_t frame=0;frame<audio.description.frames;){check(output.cancel);const auto frames=std::min<std::uint64_t>(4096,audio.description.frames-frame);const auto count=frames*audio.description.channels;
        for(std::size_t i=0;i<count;++i)block[i]=static_cast<FLAC__int32>(std::llround(std::clamp(static_cast<double>(audio.samples[frame*audio.description.channels+i])*8388608.,-8388608.,8388607.)));
        if(!FLAC__stream_encoder_process_interleaved(encoder.get(),block.data(),static_cast<unsigned>(frames)))throw std::runtime_error("FLAC encoding cancelled or failed");frame+=frames;}
    if(!FLAC__stream_encoder_finish(encoder.get()))throw std::runtime_error("FLAC encoding flush failed");
}
struct Vorbis {
    vorbis_info info{};vorbis_comment comment{};vorbis_dsp_state dsp{};vorbis_block block{};ogg_stream_state stream{};
    bool analyzed=false,blocked=false,streamed=false;
    Vorbis(){vorbis_info_init(&info);vorbis_comment_init(&comment);}
    ~Vorbis(){if(streamed)ogg_stream_clear(&stream);if(blocked)vorbis_block_clear(&block);if(analyzed)vorbis_dsp_clear(&dsp);vorbis_comment_clear(&comment);vorbis_info_clear(&info);}
};
void vorbis(Output& output,const DecodedAudio& audio,float quality){
    Vorbis state;if(vorbis_encode_init_vbr(&state.info,audio.description.channels,audio.description.sampleRate,quality)!=0)throw std::runtime_error("Unsupported Vorbis output rate or layout");
    if(vorbis_analysis_init(&state.dsp,&state.info)!=0)throw std::runtime_error("Vorbis analysis initialization failed");state.analyzed=true;
    if(vorbis_block_init(&state.dsp,&state.block)!=0)throw std::runtime_error("Vorbis block initialization failed");state.blocked=true;
    if(ogg_stream_init(&state.stream,42)!=0)throw std::runtime_error("Ogg stream initialization failed");state.streamed=true;
    vorbis_comment_add_tag(&state.comment,"ENCODER","AzureRender libvorbis 1.3.7");ogg_packet identification,comments,setup;
    if(vorbis_analysis_headerout(&state.dsp,&state.comment,&identification,&comments,&setup)!=0)throw std::runtime_error("Vorbis header creation failed");
    for(auto* packet:{&identification,&comments,&setup})if(ogg_stream_packetin(&state.stream,packet)!=0)throw std::runtime_error("Ogg packet write failed");
    ogg_page page;const auto writePage=[&]{if(!output.write(page.header,page.header_len)||!output.write(page.body,page.body_len))throw std::runtime_error("Ogg write cancelled or exceeds byte budget");};
    while(ogg_stream_flush(&state.stream,&page))writePage();
    const auto drain=[&]{while(vorbis_analysis_blockout(&state.dsp,&state.block)==1){check(output.cancel);if(vorbis_analysis(&state.block,nullptr)||vorbis_bitrate_addblock(&state.block))throw std::runtime_error("Vorbis analysis failed");ogg_packet packet;
        while(vorbis_bitrate_flushpacket(&state.dsp,&packet)){if(ogg_stream_packetin(&state.stream,&packet))throw std::runtime_error("Ogg packet submission failed");while(ogg_stream_pageout(&state.stream,&page))writePage();}}};
    for(std::uint64_t position=0;position<audio.description.frames;){check(output.cancel);const auto count=std::min<std::uint64_t>(4096,audio.description.frames-position);auto** buffer=vorbis_analysis_buffer(&state.dsp,static_cast<int>(count));if(!buffer)throw std::runtime_error("Vorbis buffer allocation failed");
        for(std::size_t i=0;i<count;++i)for(unsigned c=0;c<audio.description.channels;++c)buffer[c][i]=audio.samples[(position+i)*audio.description.channels+c];
        if(vorbis_analysis_wrote(&state.dsp,static_cast<int>(count)))throw std::runtime_error("Vorbis sample submission failed");drain();position+=count;}
    if(vorbis_analysis_wrote(&state.dsp,0))throw std::runtime_error("Vorbis final submission failed");drain();while(ogg_stream_flush(&state.stream,&page))writePage();
}
}
AssetTransferReport prepareAudioExport(const AssetTransferRequest& request,const AssetCancellation& cancel){
    const auto settings=normalizeAudioExportSettings(request.formatId,request.settings);const double quality=settings.value("quality",.6);
    const auto audio=decodeAudioFile(request.source,{settings.value("sampleRate",0U),settings.value("channels",0U)},cancel);AssetTransferReport report;report.entry=request.source.stem();report.entry+=request.formatId=="vorbis"?".ogg":"."+request.formatId;
    Output output(request.staging/report.entry,cancel);
    if(request.formatId=="wav")wave(output,audio);
    else if(request.formatId=="flac"){flac(output,audio);report.losses.push_back("Float32 samples are quantized to signed 24-bit PCM and clipped to its range");}
    else if(request.formatId=="vorbis"){vorbis(output,audio,static_cast<float>(quality));report.losses.push_back("Vorbis uses lossy perceptual coding at the declared quality");}
    else throw std::invalid_argument("Unsupported audio writer");output.finish();
    const auto description=validateAudioFile(request.staging/report.entry,cancel);report.outputs={report.entry};report.generation.outputHashes[report.entry.generic_u8string()]=assetTransferFingerprint(request.staging/report.entry,cancel);
    report.summary={{"domain","audio"},{"sampleRate",description.sampleRate},{"channels",description.channels},{"frames",description.frames},{"duration",description.duration},{"quality",quality},{"losses",report.losses}};return report;
}
nlohmann::json normalizeAudioExportSettings(const std::string& format,const nlohmann::json& settings){
    if(!settings.is_object())throw std::invalid_argument("Audio export settings must be an object");
    for(auto it=settings.begin();it!=settings.end();++it)if(it.key()!="sampleRate"&&it.key()!="channels"&&!(it.key()=="quality"&&format=="vorbis"))throw std::invalid_argument("Unknown audio export setting: "+it.key());auto normalized=settings;
    for(const std::string key:{"sampleRate","channels"}){
        if(settings.contains(key)&&!settings.at(key).is_number_integer())throw std::invalid_argument("Audio rate and channels must be integers");
        const auto value=settings.value(key,std::int64_t(0));if(value<0||(key=="channels"?value>8:value!=0&&(value<8000||value>192000)))throw std::invalid_argument("Audio conversion rate or layout is outside its budget");normalized[key]=value;}
    if(format=="vorbis"){const auto quality=settings.value("quality",.6);if(!std::isfinite(quality)||quality<0||quality>1)throw std::invalid_argument("Vorbis quality must be within [0,1]");normalized["quality"]=quality;}return normalized;
}
AssetTransferReport planAudioExport(const AssetTransferRequest& request,const AssetCancellation& cancel){
    const auto source=probeAudioFile(request.source,cancel);AssetTransferReport report;report.summary={{"domain","audio"},{"sampleRate",source.sampleRate},{"channels",source.channels},{"frames",source.frames},{"duration",source.duration}};
    if(request.formatId=="flac")report.losses.push_back("Float32 samples are quantized to signed 24-bit PCM and clipped to its range");
    if(request.formatId=="vorbis")report.losses.push_back("Vorbis uses lossy perceptual coding at the declared quality");return report;
}
}
