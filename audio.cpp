#include "audio.h"
#include <windows.h>
#include <xaudio2.h>
#include <atomic>
#include <thread>
#include <algorithm>
#include <cstdio>
#include <mutex>

struct GlassAudio::Impl : IXAudio2VoiceCallback {
 struct Command {bool config=false;SoundFrame frame;SoundDescription description;};
 std::array<Command,64>queue{};std::atomic<uint32_t>head{0},tail{0};
 std::atomic<bool>running{false},ready{false};std::atomic<uint64_t>samples{0},dropped{0};std::atomic<HRESULT>failure{S_OK};
 std::mutex settingsMutex;AudioSettings settings,appliedSettings;
 IXAudio2*engine=nullptr;IXAudio2MasteringVoice*master=nullptr;IXAudio2SourceVoice*source=nullptr;HANDLE wake=nullptr;std::thread worker;
 GlassSynth synth;std::array<std::array<float,SoundTick*4>,4>buffers{};uint32_t nextBuffer=0;SoundDescription current,pending;bool needConfig=false;
 bool push(const Command&c){auto h=head.load(std::memory_order_relaxed);auto next=(h+1)%queue.size();if(next==tail.load(std::memory_order_acquire)){dropped++;return false;}queue[h]=c;head.store(uint32_t(next),std::memory_order_release);return true;}
 bool pop(Command&c){auto t=tail.load(std::memory_order_relaxed);if(t==head.load(std::memory_order_acquire))return false;c=queue[t];tail.store((t+1)%queue.size(),std::memory_order_release);return true;}
 void run(){
  CoInitializeEx(nullptr,COINIT_MULTITHREADED);SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_ABOVE_NORMAL);
  while(running){
   XAUDIO2_VOICE_STATE state{};source->GetState(&state,XAUDIO2_VOICE_NOSAMPLESPLAYED);
   while(running&&state.BuffersQueued<3){auto&data=buffers[nextBuffer];
    for(int tick=0;tick<2;tick++){
     AudioSettings nextSettings;{std::lock_guard lock(settingsMutex);nextSettings=settings;}
     if(nextSettings!=appliedSettings){synth.setSettings(nextSettings);appliedSettings=nextSettings;}
     SoundFrame drive;drive.generation=current.generation;Command cmd;
     while(pop(cmd)){if(cmd.config){current=cmd.description;synth.configure(current);drive.generation=current.generation;continue;}drive=cmd.frame;break;}
     synth.excite(drive);synth.render(data.data()+tick*SoundTick*2,SoundTick);
    }
    XAUDIO2_BUFFER buffer{};buffer.AudioBytes=UINT32(data.size()*sizeof(float));buffer.pAudioData=reinterpret_cast<const BYTE*>(data.data());
    HRESULT hr=source->SubmitSourceBuffer(&buffer);if(FAILED(hr)){failure=hr;ready=false;running=false;break;}samples+=SoundTick*2;nextBuffer=(nextBuffer+1)%buffers.size();state.BuffersQueued++;
   }
   if(running)WaitForSingleObject(wake,30);
  }CoUninitialize();
 }
 void STDMETHODCALLTYPE OnVoiceProcessingPassStart(UINT32)override{}void STDMETHODCALLTYPE OnVoiceProcessingPassEnd()override{}
 void STDMETHODCALLTYPE OnStreamEnd()override{}void STDMETHODCALLTYPE OnBufferStart(void*)override{}
 void STDMETHODCALLTYPE OnBufferEnd(void*)override{if(wake)SetEvent(wake);}void STDMETHODCALLTYPE OnLoopEnd(void*)override{}
 void STDMETHODCALLTYPE OnVoiceError(void*,HRESULT error)override{failure=error;ready=false;running=false;if(wake)SetEvent(wake);}
};
GlassAudio::GlassAudio():impl(std::make_unique<Impl>()){}
GlassAudio::~GlassAudio(){stop();}
bool GlassAudio::start(const wchar_t*deviceId){
 if(impl->ready)return true;stop();auto&i=*impl;i.failure=S_OK;i.head=0;i.tail=0;i.nextBuffer=0;i.samples=0;i.dropped=0;
 HRESULT hr=XAudio2Create(&i.engine,0,XAUDIO2_DEFAULT_PROCESSOR);if(SUCCEEDED(hr))hr=i.engine->CreateMasteringVoice(&i.master,XAUDIO2_DEFAULT_CHANNELS,XAUDIO2_DEFAULT_SAMPLERATE,0,deviceId,nullptr,AudioCategory_GameEffects);
 WAVEFORMATEX format{};format.wFormatTag=WAVE_FORMAT_IEEE_FLOAT;format.nChannels=2;format.nSamplesPerSec=SoundRate;format.wBitsPerSample=32;format.nBlockAlign=8;format.nAvgBytesPerSec=SoundRate*8;
 if(SUCCEEDED(hr))hr=i.engine->CreateSourceVoice(&i.source,&format,0,1,&i);
 if(FAILED(hr)){i.failure=hr;stop();return false;}
 i.wake=CreateEvent(nullptr,FALSE,FALSE,nullptr);if(!i.wake){i.failure=HRESULT_FROM_WIN32(GetLastError());stop();return false;}
 hr=i.source->Start();if(FAILED(hr)){i.failure=hr;stop();return false;}
 i.running=true;i.ready=true;i.worker=std::thread([&i]{i.run();});return true;
}
void GlassAudio::stop(){if(!impl)return;auto&i=*impl;i.running=false;i.ready=false;if(i.wake)SetEvent(i.wake);if(i.worker.joinable())i.worker.join();if(i.source){i.source->DestroyVoice();i.source=nullptr;}if(i.master){i.master->DestroyVoice();i.master=nullptr;}if(i.engine){i.engine->Release();i.engine=nullptr;}if(i.wake){CloseHandle(i.wake);i.wake=nullptr;}}
void GlassAudio::configure(const SoundDescription&description){if(!available())return;impl->pending=description;Impl::Command c;c.config=true;c.description=description;impl->needConfig=!impl->push(c);}
void GlassAudio::submit(const SoundFrame&frame){if(!available())return;Impl::Command c;if(impl->needConfig){c.config=true;c.description=impl->pending;if(!impl->push(c))return;impl->needConfig=false;c.config=false;}c.frame=frame;impl->push(c);}
void GlassAudio::setSettings(const AudioSettings&settings){auto next=sanitizeAudioSettings(settings);std::lock_guard lock(impl->settingsMutex);impl->settings=next;}
bool GlassAudio::available()const{return impl->ready.load();}
uint64_t GlassAudio::renderedFrames()const{return impl->samples.load();}
uint64_t GlassAudio::droppedFrames()const{return impl->dropped.load();}
std::string GlassAudio::status()const{if(available())return "XAudio2 / 48 kHz stereo";char s[80];sprintf_s(s,"Audio unavailable (0x%08X)",unsigned(impl->failure.load()));return s;}
