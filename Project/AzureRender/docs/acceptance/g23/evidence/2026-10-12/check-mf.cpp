#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mftransform.h>
#include <iostream>
int main(){HRESULT hr=CoInitializeEx(nullptr,COINIT_MULTITHREADED);if(FAILED(hr))return 1;hr=MFStartup(MF_VERSION);if(FAILED(hr))return 2;
MFT_REGISTER_TYPE_INFO encoded{MFMediaType_Audio,MFAudioFormat_AAC},pcm{MFMediaType_Audio,MFAudioFormat_PCM};
for(bool encode:{false,true}){IMFActivate** items=nullptr;UINT32 count=0;hr=MFTEnumEx(encode?MFT_CATEGORY_AUDIO_ENCODER:MFT_CATEGORY_AUDIO_DECODER,MFT_ENUM_FLAG_ALL,encode?&pcm:&encoded,encode?&encoded:&pcm,&items,&count);std::cout<<(encode?"aacEncoders":"aacDecoders")<<"="<<count<<" status="<<std::hex<<hr<<std::dec<<"\n";for(UINT32 i=0;i<count;++i)items[i]->Release();CoTaskMemFree(items);if(FAILED(hr)||!count)return 3;}
MFShutdown();CoUninitialize();return 0;}
