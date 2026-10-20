#pragma once
#include <array>
#include <cstdint>
#include <filesystem>

struct AudioSettings {
 bool enabled=true;
 float volume=.75f,decayMs=20,impacts=1,scraping=1,walls=1,pitch=0,texture=1,onsetSpreadMs=8,stereoWidth=1;
 bool operator==(const AudioSettings&)const=default;
};
AudioSettings sanitizeAudioSettings(AudioSettings settings);
AudioSettings loadAudioSettings(const std::filesystem::path&path);
bool saveAudioSettings(const std::filesystem::path&path,const AudioSettings&settings);

constexpr int SoundVoices=102,SoundModes=6,SoundRate=48000,SoundTick=400;
constexpr int SoundLookahead=96; // 2 ms stereo-linked peak limiting.
struct SoundDrive {
 float impact=0,scrape=0,slipSpeed=0,pressure=0,x=0,y=0,z=0,pan=0;
};
struct SoundFrame {uint32_t generation=0;std::array<SoundDrive,SoundVoices> voices{};};
struct SoundVoiceSpec {
 bool enabled=false,pane=false;float pan=0;
 // decay is the amplitude e-folding time, not the audible release duration.
 std::array<float,SoundModes> frequency{},decay{},gain{};
};
struct SoundDescription {uint32_t generation=0;std::array<SoundVoiceSpec,SoundVoices> voices{};};
class CrystalScene;
SoundDescription describeSound(const CrystalScene&scene);

// Small, deterministic modal approximation; the audio and offline export share it.
class GlassSynth {
public:
 void configure(const SoundDescription&description);
 void setSettings(const AudioSettings&settings);
 void excite(const SoundFrame&frame);
 void render(float*stereo,int frames);
 uint64_t impacts=0,scrapeTicks=0,scrapeGrains=0;float maximumRaw=0;
private:
 struct Mode {float re=0,im=0,rc=0,rs=0,gain=0,drive=0;};
 struct Voice {
  std::array<Mode,SoundModes>m;
  float impact=0,scrape=0,targetScrape=0,slipSpeed=0,grainWait=0,pressure=0,pan=0,targetPan=0;
  int impactWait=0;
  bool enabled=false,pane=false;
 };
 std::array<Voice,SoundVoices>voices{};
 SoundDescription description{};AudioSettings settings{};
 struct PeakPoint {uint64_t sample=0;float peak=0;};
 std::array<PeakPoint,SoundLookahead+2>peakQueue{};
 std::array<std::array<float,2>,SoundLookahead>lookahead{};
 uint64_t audioSample=0;int peakHead=0,peakCount=0,delayCursor=0;float limiterGain=1;
 uint32_t generation=0,random=0x41b27c19;float volumeNow=0,wallNow=1,widthNow=1,lowL=0,lowR=0,dcL=0,dcR=0;
 float noise();
};
