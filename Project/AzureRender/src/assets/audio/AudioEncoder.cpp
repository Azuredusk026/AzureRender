#include "assets/audio/AudioEncoder.hpp"
#include "assets/audio/AudioCodec.hpp"
#include <FLAC/stream_encoder.h>
#include <vorbis/vorbisenc.h>
#include <opus/opus.h>
#include <lame/lame.h>
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
nlohmann::json opus(Output& output,const DecodedAudio& audio,int bitrate){
    if(audio.description.sampleRate!=48000||audio.description.channels>2)throw std::invalid_argument("Opus output requires 48000 Hz and mono or stereo");
    int error=0;std::unique_ptr<OpusEncoder,decltype(&opus_encoder_destroy)> encoder(opus_encoder_create(48000,static_cast<int>(audio.description.channels),OPUS_APPLICATION_AUDIO,&error),opus_encoder_destroy);if(!encoder||error!=OPUS_OK)throw std::runtime_error("Opus encoder creation failed");
    if(opus_encoder_ctl(encoder.get(),OPUS_SET_BITRATE(bitrate))!=OPUS_OK||opus_encoder_ctl(encoder.get(),OPUS_SET_COMPLEXITY(8))!=OPUS_OK)throw std::runtime_error("Opus encoder configuration failed");int delay=0;if(opus_encoder_ctl(encoder.get(),OPUS_GET_LOOKAHEAD(&delay))!=OPUS_OK)throw std::runtime_error("Opus lookahead query failed");
    struct Stream {ogg_stream_state value{};Stream(){if(ogg_stream_init(&value,42))throw std::runtime_error("Opus Ogg initialization failed");}~Stream(){ogg_stream_clear(&value);}} stream;
    ogg_int64_t number=0;ogg_page page{};
    const auto packet=[&](std::vector<unsigned char>& bytes,ogg_int64_t granule,bool first,bool last){ogg_packet value{};value.packet=bytes.data();value.bytes=static_cast<long>(bytes.size());value.b_o_s=first?1:0;value.e_o_s=last?1:0;value.granulepos=granule;value.packetno=number++;if(ogg_stream_packetin(&stream.value,&value))throw std::runtime_error("Opus Ogg packet submission failed");while(ogg_stream_flush(&stream.value,&page)){if(!output.write(page.header,page.header_len)||!output.write(page.body,page.body_len))throw std::runtime_error("Opus output cancelled or exceeds its byte budget");}};
    const auto word=[](std::vector<unsigned char>& bytes,unsigned value,unsigned count){for(unsigned i=0;i<count;++i)bytes.push_back(static_cast<unsigned char>(value>>(8*i)));};
    std::vector<unsigned char> head{'O','p','u','s','H','e','a','d',1,static_cast<unsigned char>(audio.description.channels)};word(head,static_cast<unsigned>(delay),2);word(head,48000,4);word(head,0,2);head.push_back(0);packet(head,0,true,false);
    std::vector<unsigned char> tags{'O','p','u','s','T','a','g','s'};const std::string vendor="AzureRender libopus 1.6.1";word(tags,static_cast<unsigned>(vendor.size()),4);tags.insert(tags.end(),vendor.begin(),vendor.end());word(tags,0,4);packet(tags,0,false,false);
    constexpr std::uint64_t blockFrames=960;const auto valid=audio.description.frames+static_cast<unsigned>(delay);const auto packetFrames=((valid+blockFrames-1)/blockFrames)*blockFrames;std::vector<float> block(static_cast<std::size_t>(blockFrames)*audio.description.channels);std::vector<unsigned char> encoded(4096);
    for(std::uint64_t at=0;at<packetFrames;at+=blockFrames){check(output.cancel);std::fill(block.begin(),block.end(),0.F);const auto count=at<audio.description.frames?std::min<std::uint64_t>(blockFrames,audio.description.frames-at):0;for(std::size_t i=0;i<count*audio.description.channels;++i)block[i]=std::clamp(audio.samples[at*audio.description.channels+i],-1.F,1.F);const auto bytes=opus_encode_float(encoder.get(),block.data(),static_cast<int>(blockFrames),encoded.data(),static_cast<opus_int32>(encoded.size()));if(bytes<=0)throw std::runtime_error("Opus packet encoding failed");encoded.resize(static_cast<std::size_t>(bytes));const bool last=at+blockFrames==packetFrames;packet(encoded,static_cast<ogg_int64_t>(last?valid:at+blockFrames),false,last);encoded.resize(4096);}
    return {{"encoderDelay",delay},{"encoderPadding",packetFrames-valid},{"effectiveFrames",audio.description.frames},{"bitrate",bitrate},{"packetFrames",960}};
}
using Mp3Encoder=std::unique_ptr<lame_global_flags,decltype(&lame_close)>;
Mp3Encoder createMp3Encoder(const AudioDescription& description,int bitrate){
    if(description.channels>2||description.sampleRate>48000)throw std::invalid_argument("MP3 requires mono or stereo and a supported rate up to 48000 Hz");
    Mp3Encoder encoder(lame_init(),lame_close);if(!encoder)throw std::runtime_error("MP3 encoder allocation failed");
    if(lame_set_num_channels(encoder.get(),static_cast<int>(description.channels))||lame_set_in_samplerate(encoder.get(),static_cast<int>(description.sampleRate))||lame_set_out_samplerate(encoder.get(),static_cast<int>(description.sampleRate))||lame_set_brate(encoder.get(),bitrate/1000)||lame_set_quality(encoder.get(),2)||lame_set_bWriteVbrTag(encoder.get(),1)||lame_init_params(encoder.get())<0)throw std::runtime_error("MP3 output rate, bitrate or layout is unsupported");
    if(lame_get_brate(encoder.get())!=bitrate/1000||lame_get_out_samplerate(encoder.get())!=static_cast<int>(description.sampleRate))throw std::invalid_argument("MP3 bitrate and rate combination requires an explicit supported setting");
    return encoder;
}
nlohmann::json mp3(Output& output,const DecodedAudio& audio,int bitrate){
    auto encoder=createMp3Encoder(audio.description,bitrate);
    lame_set_write_id3tag_automatic(encoder.get(),0);std::vector<unsigned char> bytes(16384);std::vector<float> block(8192);
    for(std::uint64_t at=0;at<audio.description.frames;){check(output.cancel);const auto count=std::min<std::uint64_t>(4096,audio.description.frames-at);for(std::size_t i=0;i<count*audio.description.channels;++i)block[i]=std::clamp(audio.samples[at*audio.description.channels+i],-1.F,1.F);
        const int size=audio.description.channels==2?lame_encode_buffer_interleaved_ieee_float(encoder.get(),block.data(),static_cast<int>(count),bytes.data(),static_cast<int>(bytes.size())):lame_encode_buffer_ieee_float(encoder.get(),block.data(),nullptr,static_cast<int>(count),bytes.data(),static_cast<int>(bytes.size()));if(size<0||!output.write(bytes.data(),static_cast<std::size_t>(size)))throw std::runtime_error("MP3 encoding cancelled or failed");at+=count;}
    const int flushed=lame_encode_flush(encoder.get(),bytes.data(),static_cast<int>(bytes.size()));if(flushed<0||!output.write(bytes.data(),static_cast<std::size_t>(flushed)))throw std::runtime_error("MP3 encoding flush failed");
    const auto tag=lame_get_lametag_frame(encoder.get(),bytes.data(),bytes.size());if(!tag||tag>bytes.size())throw std::runtime_error("MP3 gapless metadata is unavailable");output.file.seekp(0);if(!output.write(bytes.data(),tag))throw std::runtime_error("MP3 gapless metadata write failed");
    return {{"encoderDelay",lame_get_encoder_delay(encoder.get())},{"encoderPadding",lame_get_encoder_padding(encoder.get())},{"effectiveFrames",audio.description.frames},{"bitrate",bitrate}};
}

}
AssetTransferReport prepareAudioExport(const AssetTransferRequest& request,const AssetCancellation& cancel){
    const auto settings=normalizeAudioExportSettings(request.formatId,request.settings);const double quality=settings.value("quality",.6);
    const auto audio=decodeAudioFile(request.source,{request.formatId=="opus"?48000U:settings.value("sampleRate",0U),settings.value("channels",0U)},cancel);AssetTransferReport report;report.entry=request.source.stem();report.entry+=request.formatId=="vorbis"?".ogg":"."+request.formatId;
    Output output(request.staging/report.entry,cancel);nlohmann::json encoding=nlohmann::json::object();
    if(request.formatId=="wav")wave(output,audio);
    else if(request.formatId=="flac"){flac(output,audio);report.losses.push_back("Float32 samples are quantized to signed 24-bit PCM and clipped to its range");}
    else if(request.formatId=="vorbis"){vorbis(output,audio,static_cast<float>(quality));report.losses.push_back("Vorbis uses lossy perceptual coding at the declared quality");}
    else if(request.formatId=="opus"){encoding=opus(output,audio,settings.value("bitrate",128000));report.losses.push_back("Opus uses perceptual coding at 48000 Hz; tags and samples outside [-1,1] are normalized");}
    else if(request.formatId=="mp3"){encoding=mp3(output,audio,settings.value("bitrate",128000));report.losses.push_back("MP3 uses perceptual coding; source tags and samples outside [-1,1] are normalized");}
    else throw std::invalid_argument("Unsupported audio writer");output.finish();
    const auto description=validateAudioFile(request.staging/report.entry,cancel);report.outputs={report.entry};report.generation.outputHashes[report.entry.generic_u8string()]=assetTransferFingerprint(request.staging/report.entry,cancel);
    report.summary={{"domain","audio"},{"sampleRate",description.sampleRate},{"channels",description.channels},{"frames",description.frames},{"duration",description.duration},{"quality",quality},{"losses",report.losses}};report.summary.update(encoding);return report;
}
nlohmann::json normalizeAudioExportSettings(const std::string& format,const nlohmann::json& settings){
    if(!settings.is_object())throw std::invalid_argument("Audio export settings must be an object");
    for(auto it=settings.begin();it!=settings.end();++it)if(it.key()!="sampleRate"&&it.key()!="channels"&&!(it.key()=="quality"&&format=="vorbis")&&!(it.key()=="bitrate"&&(format=="opus"||format=="mp3")))throw std::invalid_argument("Unknown audio export setting: "+it.key());auto normalized=settings;
    for(const std::string key:{"sampleRate","channels"}){
        if(settings.contains(key)&&!settings.at(key).is_number_integer())throw std::invalid_argument("Audio rate and channels must be integers");
        const auto value=settings.value(key,std::int64_t(0));if(value<0||(key=="channels"?value>8:value!=0&&(value<8000||value>192000)))throw std::invalid_argument("Audio conversion rate or layout is outside its budget");normalized[key]=value;}
    if(format=="vorbis"){const auto quality=settings.value("quality",.6);if(!std::isfinite(quality)||quality<0||quality>1)throw std::invalid_argument("Vorbis quality must be within [0,1]");normalized["quality"]=quality;}
    if(format=="opus"||format=="mp3"){if(settings.contains("bitrate")&&!settings.at("bitrate").is_number_integer())throw std::invalid_argument("Audio bitrate must be an integer");const auto bitrate=settings.value("bitrate",128000);if(bitrate<6000||bitrate>(format=="opus"?512000:320000))throw std::invalid_argument("Audio bitrate exceeds the encoder range");if(normalized["channels"].get<int>()>2)throw std::invalid_argument("Compressed output supports mono and stereo");if(format=="opus"&&normalized["sampleRate"]!=0&&normalized["sampleRate"]!=48000)throw std::invalid_argument("Opus exchange output uses 48000 Hz");if(format=="mp3"){const std::vector<int> rates={8000,16000,24000,32000,40000,48000,56000,64000,80000,96000,112000,128000,144000,160000,192000,224000,256000,320000};if(std::find(rates.begin(),rates.end(),bitrate)==rates.end())throw std::invalid_argument("Unsupported MP3 bitrate");}normalized["bitrate"]=bitrate;}return normalized;
}
AssetTransferReport planAudioExport(const AssetTransferRequest& request,const AssetCancellation& cancel){
    const auto source=probeAudioFile(request.source,cancel);const auto settings=normalizeAudioExportSettings(request.formatId,request.settings);auto target=source;
    target.channels=settings.value("channels",0U)?settings.at("channels").get<unsigned>():source.channels;
    target.sampleRate=request.formatId=="opus"?48000U:(settings.value("sampleRate",0U)?settings.at("sampleRate").get<unsigned>():source.sampleRate);
    if(request.formatId=="opus"&&target.channels>2)throw std::invalid_argument("Opus output requires mono or stereo");
    if(request.formatId=="mp3")static_cast<void>(createMp3Encoder(target,settings.value("bitrate",128000)));
    target.frames=static_cast<std::uint64_t>(std::llround(double(source.frames)*target.sampleRate/source.sampleRate));
    AssetTransferReport report;report.summary={{"domain","audio"},{"sampleRate",target.sampleRate},{"channels",target.channels},{"frames",target.frames},{"duration",double(target.frames)/target.sampleRate}};
    if(request.formatId=="flac")report.losses.push_back("Float32 samples are quantized to signed 24-bit PCM and clipped to its range");
    if(request.formatId=="vorbis")report.losses.push_back("Vorbis uses lossy perceptual coding at the declared quality");if(request.formatId=="opus"||request.formatId=="mp3")report.losses.push_back("Perceptual coding normalizes source tags and samples outside [-1,1]; encoded delay and padding are recorded");return report;
}
}
