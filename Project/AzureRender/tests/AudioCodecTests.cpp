#include "assets/AssetFormatRegistry.hpp"
#include "assets/AssetTransfer.hpp"
#include "assets/audio/AudioCodec.hpp"
#include "runtime/AudioRuntime.hpp"
#include <FLAC/stream_encoder.h>
#include <vorbis/vorbisenc.h>
#include <lame/lame.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <memory>
using namespace azurerender;
namespace {
constexpr unsigned rate=24000,frames=12000;
constexpr double pi=3.14159265358979323846;
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
template<class F> void rejects(F&& action,const char* message){bool rejected=false;try{action();}catch(const std::exception&){rejected=true;}require(rejected,message);}
std::vector<short> tone(unsigned count=frames){std::vector<short> samples(count);for(unsigned i=0;i<count;++i)samples[i]=static_cast<short>(std::lround(9000*std::sin(2*pi*220*i/rate)));return samples;}
void le(std::ostream& file,std::uint32_t value,unsigned bytes){for(unsigned i=0;i<bytes;++i)file.put(static_cast<char>((value>>(i*8))&255));}
void wav(const std::filesystem::path& path,const std::vector<short>& samples){std::ofstream file(path,std::ios::binary);file<<"RIFF";le(file,36+static_cast<unsigned>(samples.size())*2,4);file<<"WAVEfmt ";le(file,16,4);le(file,1,2);le(file,1,2);le(file,rate,4);le(file,rate*2,4);le(file,2,2);le(file,16,2);file<<"data";le(file,static_cast<unsigned>(samples.size())*2,4);for(auto v:samples)le(file,static_cast<std::uint16_t>(v),2);}
void flac(const std::filesystem::path& path,const std::vector<short>& samples){
    std::unique_ptr<FLAC__StreamEncoder,decltype(&FLAC__stream_encoder_delete)> encoder(FLAC__stream_encoder_new(),FLAC__stream_encoder_delete);require(encoder!=nullptr,"Create FLAC fixture encoder");
    require(FLAC__stream_encoder_set_channels(encoder.get(),1)&&FLAC__stream_encoder_set_bits_per_sample(encoder.get(),16)&&FLAC__stream_encoder_set_sample_rate(encoder.get(),rate)&&FLAC__stream_encoder_set_total_samples_estimate(encoder.get(),samples.size()),"Configure FLAC fixture encoder");
    require(FLAC__stream_encoder_init_file(encoder.get(),path.string().c_str(),nullptr,nullptr)==FLAC__STREAM_ENCODER_INIT_STATUS_OK,"Initialize FLAC fixture");
    const std::vector<FLAC__int32> pcm(samples.begin(),samples.end());require(FLAC__stream_encoder_process_interleaved(encoder.get(),pcm.data(),static_cast<unsigned>(pcm.size()))&&FLAC__stream_encoder_finish(encoder.get()),"Encode FLAC fixture");
}
void mp3(const std::filesystem::path& path,const std::vector<short>& samples){
    lame_t encoder=lame_init();require(encoder!=nullptr,"Create MP3 fixture encoder");
    lame_set_num_channels(encoder,1);lame_set_in_samplerate(encoder,rate);lame_set_out_samplerate(encoder,rate);lame_set_brate(encoder,96);lame_set_quality(encoder,2);lame_set_num_samples(encoder,static_cast<unsigned long>(samples.size()));
    require(lame_init_params(encoder)>=0,"Initialize MP3 fixture");std::vector<unsigned char> bytes(samples.size()*2+7200);
    const int count=lame_encode_buffer(encoder,samples.data(),samples.data(),static_cast<int>(samples.size()),bytes.data(),static_cast<int>(bytes.size()));require(count>=0,"Encode MP3 samples");
    const int tail=lame_encode_flush(encoder,bytes.data()+count,static_cast<int>(bytes.size())-count);require(tail>=0,"Flush MP3 samples");
    FILE* file=std::fopen(path.string().c_str(),"wb+");require(file!=nullptr,"Create MP3 fixture");require(std::fwrite(bytes.data(),1,count+tail,file)==static_cast<std::size_t>(count+tail),"Write MP3 fixture");lame_mp3_tags_fid(encoder,file);std::fclose(file);lame_close(encoder);
}
void vorbis(const std::filesystem::path& path,const std::vector<short>& samples){
    vorbis_info info;vorbis_info_init(&info);require(vorbis_encode_init_vbr(&info,1,rate,.6F)==0,"Configure Vorbis fixture");
    vorbis_comment comment;vorbis_comment_init(&comment);vorbis_dsp_state dsp;require(vorbis_analysis_init(&dsp,&info)==0,"Initialize Vorbis analysis");vorbis_block block;vorbis_block_init(&dsp,&block);ogg_stream_state stream;ogg_stream_init(&stream,42);
    std::ofstream file(path,std::ios::binary);ogg_packet identification,comments,setup;vorbis_analysis_headerout(&dsp,&comment,&identification,&comments,&setup);ogg_stream_packetin(&stream,&identification);ogg_stream_packetin(&stream,&comments);ogg_stream_packetin(&stream,&setup);
    const auto page=[&](const ogg_page& value){file.write(reinterpret_cast<const char*>(value.header),value.header_len);file.write(reinterpret_cast<const char*>(value.body),value.body_len);};ogg_page output;
    while(ogg_stream_flush(&stream,&output))page(output);
    const auto drain=[&]{while(vorbis_analysis_blockout(&dsp,&block)==1){vorbis_analysis(&block,nullptr);vorbis_bitrate_addblock(&block);ogg_packet packet;
        while(vorbis_bitrate_flushpacket(&dsp,&packet)){ogg_stream_packetin(&stream,&packet);while(ogg_stream_pageout(&stream,&output))page(output);}}};
    for(std::size_t position=0;position<samples.size();){const auto count=std::min<std::size_t>(4096,samples.size()-position);
        float** buffer=vorbis_analysis_buffer(&dsp,static_cast<int>(count));require(buffer!=nullptr,"Allocate Vorbis analysis block");
        for(std::size_t i=0;i<count;++i)buffer[0][i]=samples[position+i]/32768.F;
        require(vorbis_analysis_wrote(&dsp,static_cast<int>(count))==0,"Submit Vorbis analysis block");drain();position+=count;}
    vorbis_analysis_wrote(&dsp,0);drain();
    ogg_stream_clear(&stream);vorbis_block_clear(&block);vorbis_dsp_clear(&dsp);vorbis_comment_clear(&comment);vorbis_info_clear(&info);require(static_cast<bool>(file),"Write Vorbis fixture");
}
double rms(const std::vector<float>& samples){double sum=0;for(float sample:samples)sum+=sample*sample;return std::sqrt(sum/samples.size());}
double power(const DecodedAudio& audio,double frequency){double real=0,imaginary=0;for(std::size_t i=0;i<audio.description.frames;++i){const auto value=audio.samples[i*audio.description.channels];const double phase=2*pi*frequency*i/audio.description.sampleRate;real+=value*std::cos(phase);imaginary+=value*std::sin(phase);}return real*real+imaginary*imaginary;}
}
int main(int argc,char** argv){const auto root=std::filesystem::temp_directory_path()/("azure-audio-codec-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));try{
    std::filesystem::create_directories(root);
    const auto& registry=azurerender::assetFormatRegistry();
    for(const std::string id:{"wav","mp3","flac","vorbis"})
        if(!registry.require(id).canRead)throw std::runtime_error("Audio formats require production readers");
    const auto samples=tone();wav(root/"tone.wav",samples);flac(root/"tone.flac",samples);mp3(root/"tone.mp3",samples);vorbis(root/"tone.ogg",samples);
    const auto wave=decodeAudioFile(root/"tone.wav");require(wave.description.frames==frames&&wave.description.channels==1&&wave.description.sampleRate==rate,"WAV frame count and native format");
    for(std::size_t i=0;i<samples.size();++i)require(std::abs(wave.samples[i]-samples[i]/32768.F)<1e-6F,"WAV samples preserve independent signed PCM fixture");
    require(decodeAudioFile(root/"tone.flac").samples==wave.samples,"FLAC lossless PCM equals the independent WAV fixture");
    for(const std::string file:{"tone.wav","tone.flac","tone.mp3","tone.ogg"}){
        const auto path=root/file;const auto native=decodeAudioFile(path);require(native.description.channels==1&&native.description.sampleRate==rate&&std::abs(native.description.duration-.5)<.07,"Native audio duration and format");
        require(rms(native.samples)>.16&&rms(native.samples)<.23,"Encoded tone has the expected amplitude");require(power(native,220)>1000*power(native,440),"Encoded tone preserves 220 Hz spectrum");
        const auto converted=decodeAudioFile(path,{48000,2});require(converted.description.channels==2&&converted.description.sampleRate==48000&&std::abs(converted.description.duration-.5)<.07,"Rate and stereo conversion preserves time");
        for(std::size_t i=0;i<converted.samples.size();i+=2)require(std::abs(converted.samples[i]-converted.samples[i+1])<1e-6F,"Mono conversion fills both stereo channels equally");
        AssetTransferRequest request;request.source=path;request.staging=root/(file+"-stage");request.destination=root/(file+"-published");auto report=prepareAssetTransfer(request,registry,{});
        require(report.summary.at("domain")=="audio"&&report.summary.at("strategy")=="predecode","Audio import exposes native description and decode strategy");const auto published=publishAssetTransfer(report);require(probeAudioFile(published).channels==1,"Published audio remains independently decodable");std::filesystem::remove_all(request.destination);
        AudioRuntime runtime(false);const auto handle=runtime.load(path,true,1);runtime.play(handle);const auto mixed=runtime.mix(48000);require(rms(mixed)>.14&&runtime.playing(handle),"Offline production mixer plays and loops each input format");runtime.release(handle);runtime.release(handle);require(!runtime.playing(handle),"Repeated release is safe");
    }
    rejects([&]{decodeAudioFile(root/"tone.wav",{48000,2,16});},"Decoded byte budget is enforced before allocation");
    rejects([&]{decodeAudioFile(root/"tone.wav",{},[]{return true;});},"Audio cancellation rejects work");
    {std::ifstream input(root/"tone.wav",std::ios::binary);std::vector<char> bytes((std::istreambuf_iterator<char>(input)),{});bytes.resize(bytes.size()/2);std::ofstream file(root/"truncated.wav",std::ios::binary);file.write(bytes.data(),bytes.size());}
    rejects([&]{decodeAudioFile(root/"truncated.wav");},"Truncated WAV container is rejected");
    {std::ofstream file(root/"wrong.mp3");file<<"invalid audio";}rejects([&]{registry.detect(root/"wrong.mp3");},"Disguised audio fails content detection");
    wav(root/"silence.wav",std::vector<short>(frames));require(rms(decodeAudioFile(root/"silence.wav").samples)==0,"Silent PCM stays silent");
    {std::vector<short> impulse(frames);impulse[137]=32767;wav(root/"impulse.wav",impulse);const auto decoded=decodeAudioFile(root/"impulse.wav");require(decoded.samples[137]==32767/32768.F&&decoded.samples[136]==0&&decoded.samples[138]==0,"Impulse timing and adjacent silence remain exact");}
    rejects([&]{decodeAudioFile(root/"tone.wav",{192001,1});},"Audio rate conversion rejects unsupported rates");
    rejects([&]{decodeAudioFile(root/"tone.wav",{48000,9});},"Audio channel conversion rejects unsupported layouts");
    for(const std::string format:{"flac","mp3","ogg"}){std::ifstream input(root/("tone."+format),std::ios::binary);std::vector<char> bytes((std::istreambuf_iterator<char>(input)),{});bytes.resize(bytes.size()/2);const auto path=root/("truncated."+format);std::ofstream file(path,std::ios::binary);file.write(bytes.data(),bytes.size());file.close();rejects([&]{validateAudioFile(path);},"Truncated encoded container is rejected by full sample validation");}
    wav(root/"long.wav",tone(rate*31));require(probeAudioFile(root/"long.wav").streaming,"Long audio uses the declared stream policy");
    vorbis(root/"long.ogg",tone(rate*31));require(validateAudioFile(root/"long.ogg").streaming,"Long Vorbis validates all frames with bounded PCM blocks");
    {unsigned checks=0;rejects([&]{validateAudioFile(root/"long.ogg",[&]{return ++checks>12;});},"Cancellation interrupts full stream validation");require(checks>12,"Cancellation reaches ongoing work");}
    {AudioRuntime runtime(false);const auto handle=runtime.load(root/"long.ogg",true,.5F);require(runtime.description(handle).streaming,"Long Vorbis uses actual runtime streaming");runtime.play(handle);require(rms(runtime.mix(4096))>.07,"Vorbis stream supplies decoded samples");runtime.release(handle);require(!runtime.playing(handle),"Vorbis stream releases ownership");}
    {AudioRuntime runtime(false);const auto handle=runtime.load(root/"long.wav",false,.5F);require(runtime.description(handle).streaming,"Runtime owns an actual streaming data source for long audio");runtime.play(handle);require(rms(runtime.mix(4096))>.07,"Long stream supplies actual offline samples");runtime.pauseAll(true);require(!runtime.playing(handle),"Streaming sound participates in pause");runtime.pauseAll(false);require(runtime.playing(handle),"Streaming pause resumes sound ownership");runtime.release(handle);}
    const auto unicode=root/std::filesystem::u8path(u8"🎵 音频 样本.FLAC");std::filesystem::copy_file(root/"tone.flac",unicode);require(decodeAudioFile(unicode).samples==wave.samples,"Audio codec accepts Unicode uppercase paths");
    {AudioRuntime runtime(false);const auto handle=runtime.load(unicode,false,1);runtime.play(handle);require(rms(runtime.mix(4096))>.14,"Production audio load accepts Unicode paths");runtime.release(handle);}
    if(argc==2){const auto output=std::filesystem::u8path(argv[1]);std::filesystem::create_directories(output);for(const auto& file:std::filesystem::directory_iterator(root))if(file.is_regular_file())std::filesystem::copy_file(file.path(),output/file.path().filename(),std::filesystem::copy_options::overwrite_existing);}
    std::filesystem::remove_all(root);std::cout<<"Four audio readers, PCM, rate, channels, loop, stream and ownership passed\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';std::filesystem::remove_all(root);return 1;}}
