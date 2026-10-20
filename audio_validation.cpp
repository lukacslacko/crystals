#include "scene.h"
#include "audio.h"
#include <windows.h>
#include <objbase.h>
#include <fstream>
#include <iostream>
#include <vector>
#include <filesystem>
#include <stdexcept>
#include <chrono>
#include <thread>
#include <set>

static void require(bool ok,const char*message){if(!ok)throw std::runtime_error(message);}
static double rms(const std::vector<float>&x,size_t first,size_t last){double sum=0;for(size_t i=first;i<last;i++)sum+=double(x[i])*x[i];return std::sqrt(sum/std::max(size_t(1),last-first));}
static float peak(const std::vector<float>&x){float p=0;for(float v:x){require(std::isfinite(v),"Non-finite audio sample");p=std::max(p,std::abs(v));}return p;}
static void wav(const std::filesystem::path&path,const std::vector<float>&x){std::ofstream out(path,std::ios::binary);require(bool(out),"Cannot create audio preview");auto word=[&](uint32_t v,int n){for(int i=0;i<n;i++)out.put(char(v>>(8*i)));};out.write("RIFF",4);word(36+uint32_t(x.size()*2),4);out.write("WAVEfmt ",8);word(16,4);word(1,2);word(2,2);word(SoundRate,4);word(SoundRate*4,4);word(4,2);word(16,2);out.write("data",4);word(uint32_t(x.size()*2),4);for(float v:x)word(uint16_t(int16_t(std::round(std::clamp(v,-1.f,1.f)*32767))),2);}
int main(int argc,char**argv){try{
 CrystalScene scene;scene.create();auto desc=describeSound(scene);GlassSynth synth;synth.configure(desc);std::vector<float>block(SoundTick*2);SoundFrame drive;drive.generation=scene.soundGeneration;
 // Positions follow the viewing direction and camera pan, not fixed world X.
 scene.soundCamera.yaw=0;V3 rightPoint=scene.boxCenter+V3{scene.boxHalf,0,0};require(scene.soundPan(rightPoint)>.99f,"Right-hand contact is not panned right");
 scene.soundCamera.yaw=3.14159265f;require(scene.soundPan(rightPoint)<-.99f,"Orbiting behind the cube did not reverse stereo placement");
 scene.soundCamera.yaw=0;scene.soundCamera.target=rightPoint;require(std::abs(scene.soundPan(rightPoint))<1e-5f,"Panning the camera did not move the sound center");scene.soundCamera=Camera{};
 auto stereoEnergy=[&](int id,float pan,float width,bool scrape){
  GlassSynth test;test.configure(desc);AudioSettings settings;settings.volume=.05f;settings.onsetSpreadMs=0;settings.decayMs=100;settings.stereoWidth=width;test.setSettings(settings);
  for(int i=0;i<120;i++)test.render(block.data(),SoundTick);
  std::array<double,2>energy{};
  for(int tick=0;tick<36;tick++){SoundFrame packet;packet.generation=desc.generation;
   if(tick==0||scrape){packet.voices[id].pan=pan;if(scrape){packet.voices[id].scrape=.3f;packet.voices[id].slipSpeed=1.5f;}else packet.voices[id].impact=.2f;}
   test.excite(packet);test.render(block.data(),SoundTick);require(peak(block)<.8f,"Stereo measurement hit the limiter");
   for(int n=0;n<SoundTick;n++)for(int channel=0;channel<2;channel++)energy[channel]+=double(block[n*2+channel])*block[n*2+channel];
  }return energy;
 };
 for(int id:{0,96})for(bool scrape:{false,true}){
  auto left=stereoEnergy(id,-.8f,1,scrape),right=stereoEnergy(id,.8f,1,scrape),center=stereoEnergy(id,0,1,scrape),mono=stereoEnergy(id,.8f,0,scrape),wide=stereoEnergy(id,.6f,2,scrape);
  require(left[0]>1e-5&&right[1]>1e-5&&std::abs(left[0]/left[1]-9)<.01&&std::abs(right[1]/right[0]-9)<.01,"Stereo impact/scrape or its decay lost left/right separation");
  require(std::abs(center[0]-center[1])<1e-9&&std::abs(mono[0]-mono[1])<1e-9,"Centered sound or zero stereo width is not balanced mono");
  require(std::abs((left[0]+left[1])/(center[0]+center[1])-1)<.001&&std::abs((right[0]+right[1])/(center[0]+center[1])-1)<.001,"Panning changed total sound energy");
  require(wide[1]>1e-5&&wide[0]<1e-12,"Stereo-width control failed to widen the image");
 }
 float low=1e9f,high=0;for(int i=0;i<scene.density;i++){low=std::min(low,desc.voices[i].frequency[0]);high=std::max(high,desc.voices[i].frequency[0]);}require(high-low>80,"Shape-dependent frequencies did not vary");
 for(int i=0;i<120;i++){for(auto&v:drive.voices)v.pressure=1;synth.excite(drive);synth.render(block.data(),SoundTick);require(peak(block)==0,"Resting contact produces sound");}
 // Test the requested release on EVERY crystal and wall, not just eventual silence.
 double worstRelease=-1000,worstResidual=0;
 for(int id=0;id<SoundVoices;id++)if(desc.voices[id].enabled){
  GlassSynth strike;strike.configure(desc);AudioSettings measure;measure.volume=.1f;measure.onsetSpreadMs=0;strike.setSettings(measure);for(int warm=0;warm<24;warm++)strike.render(block.data(),SoundTick);
  SoundFrame hit;hit.generation=desc.generation;hit.voices[id].impact=1;strike.excite(hit);
  std::vector<float>ring(SoundRate/10*2);strike.render(ring.data(),SoundRate/10);
  size_t onset=SoundLookahead*2;double attack=rms(ring,onset,onset+SoundRate*2/250),late=rms(ring,onset+SoundRate*2/50,onset+SoundRate*2/40);
  require(attack>1e-5,"Impact is silent");double release=20*std::log10(std::max(1e-15,late/attack));worstRelease=std::max(worstRelease,release);
  double residual=rms(ring,onset+SoundRate*2/25,ring.size())/attack;worstResidual=std::max(worstResidual,residual);
  if(release>=-40||residual>=.0005)std::cout<<"Release diagnostic: voice "<<id<<"; "<<release<<" dB; residual "<<residual<<"; tau "<<desc.voices[id].decay[0]<<" s\n";
 }
 require(worstRelease<-40,"Impact still rings beyond 20 ms");require(worstResidual<.0005,"Impact has a long tail beyond 40 ms");
 // Keep identical contact/RNG histories, but solo one body at a time to measure
 // its actual rendered onset. A same-tick pile-up must spread across the tick.
 auto onsetSpread=[&](float spread){std::set<int>starts;for(int solo=0;solo<16;solo++){
  auto soloDesc=desc;for(int id=0;id<SoundVoices;id++)if(id!=solo)soloDesc.voices[id].gain.fill(0);
  GlassSynth test;test.configure(soloDesc);AudioSettings settings;settings.volume=.1f;settings.onsetSpreadMs=spread;test.setSettings(settings);
  for(int warm=0;warm<24;warm++)test.render(block.data(),SoundTick);
  SoundFrame pile;pile.generation=desc.generation;for(auto&v:pile.voices)v.impact=.1f;test.excite(pile);
  std::vector<float>x(SoundTick*4);test.render(x.data(),SoundTick*2);int first=-1;
  for(int i=0;i<SoundTick*2;i++)if(std::abs(x[i*2])+std::abs(x[i*2+1])>1e-7){first=i;break;}
  require(first>=SoundLookahead&&first<=SoundLookahead+int(spread*48)+2,"Impact onset outside jitter/latency bounds");starts.insert(first-SoundLookahead);
 }return starts;};
 auto aligned=onsetSpread(0),scattered=onsetSpread(8);require(aligned.size()==1&&scattered.size()>=12&&*scattered.rbegin()-*scattered.begin()>240,"Simultaneous impacts remain synchronized");
 // Sliding is a sequence of weak contacts. Their density follows actual slip speed.
 auto scrapeRun=[&](float speed){GlassSynth scrape;scrape.configure(desc);SoundFrame slip;slip.generation=desc.generation;slip.voices[0].scrape=.25f;slip.voices[0].slipSpeed=speed;double level=0;
  for(int tick=0;tick<120;tick++){scrape.excite(slip);scrape.render(block.data(),SoundTick);level+=rms(block,0,block.size());}
  if(speed>0)require(level/120>1e-6,"Sliding exciter is silent");else require(level==0&&scrape.scrapeGrains==0,"Scrape sounds without sliding");
  SoundFrame quiet;quiet.generation=desc.generation;scrape.excite(quiet);auto grains=scrape.scrapeGrains;std::vector<float>tail(SoundRate/10*2);scrape.render(tail.data(),SoundRate/10);
  require(scrape.scrapeGrains==grains,"Scraping continues after contact stops");require(rms(tail,SoundLookahead*2+SoundRate*2/25,tail.size())<1e-5,"Scraping remains above -100 dBFS after 40 ms");return grains;
 };
 auto slowGrains=scrapeRun(.1f),fastGrains=scrapeRun(2.f);scrapeRun(0);require(fastGrains>slowGrains*4,"Scrape density does not follow sliding speed");
 drive={};drive.generation=desc.generation;
 GlassSynth stress;stress.configure(desc);AudioSettings loud;loud.volume=1;loud.impacts=loud.scraping=loud.walls=3;loud.decayMs=500;loud.pitch=12;loud.texture=3;stress.setSettings(loud);float stressPeak=0;
 for(int tick=0;tick<360;tick++){for(auto&v:drive.voices){v.impact=1.5f;v.scrape=.6f;v.slipSpeed=3;}stress.excite(drive);stress.render(block.data(),SoundTick);stressPeak=std::max(stressPeak,peak(block));}require(stressPeak<=.85001f,"Soft peak ceiling exceeded");
 loud.enabled=false;stress.setSettings(loud);for(int i=0;i<120;i++)stress.render(block.data(),SoundTick);require(peak(block)<1e-7,"Mute did not fade to silence");
 auto strikeWith=[&](AudioSettings settings,int id=0){GlassSynth test;test.configure(desc);test.setSettings(settings);for(int i=0;i<120;i++)test.render(block.data(),SoundTick);SoundFrame tap;tap.generation=desc.generation;tap.voices[id].impact=.1f;test.excite(tap);std::vector<float>result(SoundRate*2);test.render(result.data(),SoundRate);peak(result);return result;};
 auto releaseMs=[](const std::vector<float>&x){double total=0,partial=0;size_t first=0;while(first<x.size()&&std::abs(x[first])<1e-7)first++;for(float s:x)total+=double(s)*s;for(size_t i=first;i<x.size();i++){partial+=double(x[i])*x[i];if(partial>=total*.9999)return float(i-first)*500/SoundRate;}return 1000.f;};
 AudioSettings shortSettings,longSettings;shortSettings.decayMs=5;longSettings.decayMs=100;
 auto shortRing=strikeWith(shortSettings),longRing=strikeWith(longSettings);float shortMs=releaseMs(shortRing),longMs=releaseMs(longRing);require(longMs>shortMs*10&&shortMs<12&&longMs>60,"Decay control does not change audible release");
 AudioSettings noImpact;noImpact.impacts=0;require(peak(strikeWith(noImpact))==0,"Impact level zero is not silent");
 AudioSettings noWall;noWall.walls=0;require(peak(strikeWith(noWall,96))<1e-7,"Wall level zero is not silent");
 GlassSynth noScrape;noScrape.configure(desc);AudioSettings scrapingOff;scrapingOff.scraping=0;noScrape.setSettings(scrapingOff);SoundFrame slip;slip.generation=desc.generation;slip.voices[0].scrape=.6f;slip.voices[0].slipSpeed=2;noScrape.excite(slip);noScrape.render(block.data(),SoundTick);require(peak(block)==0,"Scrape level zero is not silent");
 AudioSettings lowPitch,highPitch;lowPitch.pitch=-12;highPitch.pitch=12;auto dark=strikeWith(lowPitch),bright=strikeWith(highPitch);
 auto crossings=[](const std::vector<float>&x){int n=0;for(int i=2;i<960;i+=2)if(x[i]*x[i-2]<0)n++;return n;};require(crossings(bright)>crossings(dark)*2,"Pitch control did not change audio frequency");
 // Saved values must survive a restart, and invalid input must stay bounded.
 auto settingsFile=std::filesystem::temp_directory_path()/std::filesystem::path("CrystalAudioValidation-"+std::to_string(GetCurrentProcessId())+".ini");
 AudioSettings custom;custom.volume=.63f;custom.decayMs=137;custom.impacts=1.4f;custom.scraping=.2f;custom.walls=0;custom.pitch=-5;custom.texture=1.8f;custom.onsetSpreadMs=3.5f;custom.stereoWidth=1.6f;custom.enabled=false;
 require(saveAudioSettings(settingsFile,custom)&&loadAudioSettings(settingsFile)==custom,"Audio settings failed to persist");std::filesystem::remove(settingsFile);
 custom.volume=100;custom.decayMs=-2;custom.pitch=100;custom.onsetSpreadMs=40;custom.stereoWidth=10;auto bounded=sanitizeAudioSettings(custom);require(bounded.volume==1&&bounded.decayMs==5&&bounded.pitch==12&&bounded.onsetSpreadMs==8&&bounded.stereoWidth==2,"Audio settings bounds failed");
 GlassSynth actual;actual.configure(desc);std::vector<float>preview(24*SoundRate*2);int tick=0;uint64_t crystalHits=0,paneHits=0,slips=0;
 scene.soundSink=[&](const SoundFrame&f){for(int i=0;i<SoundVoices;i++){if(f.voices[i].impact>.001f){if(i<96)crystalHits++;else paneHits++;}if(f.voices[i].scrape>.001f)slips++;}actual.excite(f);actual.render(preview.data()+tick*SoundTick*2,SoundTick);tick++;};
 for(int i=0;i<2160;i++)scene.step(1.f/120);scene.paused=true;scene.soundSink={};SoundFrame quiet;quiet.generation=scene.soundGeneration;
 while(tick<2880){actual.excite(quiet);actual.render(preview.data()+tick*SoundTick*2,SoundTick);tick++;}
 require(crystalHits>10&&paneHits>5&&slips>20,"Physics contacts did not excite crystals, panes and scraping");
 float p=peak(preview);double level=rms(preview,0,18*SoundRate*2);require(p>.1f&&p<=.85001f&&level>.025&&level<.3,"Default mix is too quiet or outside expected range");require(rms(preview,23*SoundRate*2,preview.size())<level*.03,"Pause does not decay away");
 std::filesystem::path output=argc>1?argv[1]:"GlassAmbient.wav";wav(output,preview);
 std::cout<<"PASS: resting silence, shape tuning, 20 ms release for every crystal/pane, speed-driven scrape grains, scrape stop, mute, finite samples, peak ceiling, real contact audio.\n";
 std::cout<<"Stereo: PASS camera-relative placement, 9:1 channel energy for +/-0.8 pan, balanced center/mono, equal total power, retained impact tails, scrape and wall placement, saved stereo width.\n";
 std::cout<<"Crystal fundamentals: "<<low<<" to "<<high<<" Hz; worst 20-25 ms release "<<worstRelease<<" dB; residual RMS after 40 ms "<<worstResidual<<" relative to attack\n";
 std::cout<<"Scrape grains/second: "<<slowGrains<<" at 0.1 units/s; "<<fastGrains<<" at 2 units/s; physical mix grains "<<actual.scrapeGrains<<"\n";
 std::cout<<"Contacts: crystal hits "<<crystalHits<<"; pane hits "<<paneHits<<"; scrape ticks "<<slips<<"\n";
 std::cout<<"Settings: PASS decay, pitch, separate impact/scrape/wall levels, saved preferences, bounds; measured releases "<<shortMs<<" ms (5 ms setting), "<<longMs<<" ms (100 ms setting)\n";
 std::cout<<"Onset spread: "<<scattered.size()<<" unique starts for 16 simultaneous impacts; "<<*scattered.begin()/48.f<<" to "<<*scattered.rbegin()/48.f<<" ms; zero-spread alignment PASS; limiter latency "<<SoundLookahead/48.f<<" ms\n";
 std::cout<<"Ambient preview: "<<output.string()<<"; 24 s stereo 48 kHz; peak "<<20*std::log10(p)<<" dBFS; active RMS "<<20*std::log10(level)<<" dBFS; stress peak "<<stressPeak<<"\n";
 if(argc>2&&std::string(argv[2])=="--device-test"){CoInitializeEx(nullptr,COINIT_MULTITHREADED);GlassAudio audio;require(audio.start(),"XAudio2 output initialization failed");audio.configure(desc);std::this_thread::sleep_for(std::chrono::seconds(2));require(audio.available()&&audio.renderedFrames()>SoundRate,"Audio streaming stalled");std::cout<<"Device: "<<audio.status()<<"; submitted "<<audio.renderedFrames()<<" frames; dropped "<<audio.droppedFrames()<<"\n";audio.stop();CoUninitialize();}
 return 0;
}catch(const std::exception&e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}}
