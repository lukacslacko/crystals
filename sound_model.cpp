#include "sound_model.h"
#include "scene.h"
#include <cmath>
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>

static constexpr float Pi=3.14159265359f;
AudioSettings sanitizeAudioSettings(AudioSettings s){
 auto limit=[](float v,float lo,float hi,float fallback){return std::isfinite(v)?std::clamp(v,lo,hi):fallback;};
 s.volume=limit(s.volume,0,1,.75f);s.decayMs=limit(s.decayMs,5,500,20);
 s.impacts=limit(s.impacts,0,3,1);s.scraping=limit(s.scraping,0,3,1);s.walls=limit(s.walls,0,3,1);
 s.pitch=limit(s.pitch,-12,12,0);s.texture=limit(s.texture,.25f,3,1);s.onsetSpreadMs=limit(s.onsetSpreadMs,0,8,8);return s;
}
AudioSettings loadAudioSettings(const std::filesystem::path&path){
 AudioSettings s;std::ifstream in(path);std::string line;
 while(std::getline(in,line)){std::istringstream row(line);std::string key;float value;if(!(row>>key>>value))continue;
  if(key=="enabled")s.enabled=value!=0;else if(key=="volume")s.volume=value;else if(key=="decay_ms")s.decayMs=value;
  else if(key=="impacts")s.impacts=value;else if(key=="scraping")s.scraping=value;else if(key=="walls")s.walls=value;
  else if(key=="pitch")s.pitch=value;else if(key=="texture")s.texture=value;
  else if(key=="onset_spread_ms")s.onsetSpreadMs=value;
 }return sanitizeAudioSettings(s);
}
bool saveAudioSettings(const std::filesystem::path&path,const AudioSettings&input){
 auto s=sanitizeAudioSettings(input);std::ofstream out(path);if(!out)return false;out<<std::setprecision(9)
  <<"enabled "<<s.enabled<<"\nvolume "<<s.volume<<"\ndecay_ms "<<s.decayMs<<"\nimpacts "<<s.impacts
  <<"\nscraping "<<s.scraping<<"\nwalls "<<s.walls<<"\npitch "<<s.pitch<<"\ntexture "<<s.texture<<"\nonset_spread_ms "<<s.onsetSpreadMs<<"\n";return bool(out);
}
SoundDescription describeSound(const CrystalScene&s){
 SoundDescription d;d.generation=s.soundGeneration;
 for(auto&c:s.cells){auto&v=d.voices[c.id];v.enabled=!c.removed;
  V3 lo{1e6f,1e6f,1e6f},hi{-1e6f,-1e6f,-1e6f};float volume=0;for(auto&t:c.local)volume+=dot(t.a.xyz(),cross(t.b.xyz(),t.c.xyz()))/6;
  for(auto p:c.hull){lo={std::min(lo.x,p.x),std::min(lo.y,p.y),std::min(lo.z,p.z)};hi={std::max(hi.x,p.x),std::max(hi.y,p.y),std::max(hi.z,p.z)};}
  auto span=hi-lo;float axes[]={span.x,span.y,span.z};std::sort(axes,axes+3);float size=std::cbrt(std::max(.05f,volume)),aspect=axes[2]/std::max(.1f,axes[0]);
  float base=std::clamp(2400.f/size*(.94f+.045f*aspect),1300.f,4000.f);float ratios[]={1,1.47f,2.09f,2.81f,3.68f,4.77f};
  for(int k=0;k<SoundModes;k++){float shape=1+.025f*std::sin(float(c.hull.size())*.37f+k*1.71f)+.025f*(axes[k%3]/size-1);v.frequency[k]=std::min(11500.f,base*ratios[k]*shape);v.decay[k]=std::clamp(.0034f+.0005f*size,.0035f,.0045f)/(1+.16f*k);v.gain[k]=.48f/std::pow(1.f+k,1.05f);}
  // Small modal approximations retain shape tuning while changing material response.
  int finish=s.appearance.finish;const float pitch[]={1,.24f,.58f,.85f},release[]={1,.38f,.65f,.24f},level[]={1,.65f,.8f,.5f};
  for(int k=0;k<SoundModes;k++){v.frequency[k]*=pitch[finish];v.decay[k]*=release[finish];v.gain[k]*=level[finish]/(1+(finish==1?.45f:finish==3?.12f:0)*k);}
  v.pan=std::clamp(c.body->getWorldTransform().getOrigin().x()/9.f,-.7f,.7f);
 }
 for(int p=0;p<6;p++){auto&v=d.voices[96+p];v.enabled=true;v.pane=true;float ratios[]={1,1.62f,2.36f,3.23f,4.31f,5.71f};float base=1050.f*(10.4f/(2*s.boxHalf))*(1+.018f*p);
  for(int k=0;k<SoundModes;k++){v.frequency[k]=base*ratios[k];v.decay[k]=.0038f/(1+.18f*k);v.gain[k]=.30f/std::pow(1.f+k,.95f);}
  int finish=p<4?s.appearance.walls[p]:0;const float pitch[]={1,.75f,.48f,1.2f},release[]={1,.22f,.6f,1.8f},level[]={1,.55f,.8f,.9f};
  for(int k=0;k<SoundModes;k++){v.frequency[k]*=pitch[finish];v.decay[k]*=release[finish];v.gain[k]*=level[finish];}
 }return d;
}
float GlassSynth::noise(){random^=random<<13;random^=random>>17;random^=random<<5;return float(random>>8)*(2.f/16777216.f)-1;}
void GlassSynth::configure(const SoundDescription&d){
 description=d;
 if(generation!=d.generation){voices={};generation=d.generation;lowL=lowR=dcL=dcR=0;lookahead={};peakHead=peakCount=delayCursor=0;audioSample=0;limiterGain=1;}
 for(int i=0;i<SoundVoices;i++){auto&v=voices[i];auto&s=d.voices[i];v.enabled=s.enabled;v.pane=s.pane;v.targetPan=s.pan;
  if(!v.enabled){v={};continue;}
  for(int k=0;k<SoundModes;k++){float frequency=std::clamp(s.frequency[k]*std::exp2(settings.pitch/12),40.f,16000.f),decay=std::max(.0001f,s.decay[k]*settings.decayMs/20);float r=std::exp(-1/(SoundRate*decay)),angle=2*Pi*frequency/SoundRate;v.m[k].rc=r*std::cos(angle);v.m[k].rs=r*std::sin(angle);v.m[k].gain=s.gain[k];}
 }
}
void GlassSynth::setSettings(const AudioSettings&input){auto next=sanitizeAudioSettings(input);bool retune=next.decayMs!=settings.decayMs||next.pitch!=settings.pitch;settings=next;if(retune)configure(description);}
void GlassSynth::excite(const SoundFrame&frame){
 if(frame.generation!=generation){for(auto&v:voices){v.targetScrape=0;v.slipSpeed=0;v.pressure=0;}return;}
 for(int i=0;i<SoundVoices;i++){auto&v=voices[i];const auto&d=frame.voices[i];if(!v.enabled)continue;v.targetPan=std::clamp(d.pan,-.75f,.75f);v.pressure=std::clamp(d.pressure,0.f,1.f);
  float hit=std::clamp(d.impact,0.f,1.5f);
  // Independent sub-tick offsets break the common 120 Hz onset grid. The 8 ms
  // maximum is shorter than one physics tick, so pending hits cannot overlap it.
  if(hit>0){if(v.impact==0)v.impactWait=int((.5f+.5f*noise())*settings.onsetSpreadMs*(SoundRate/1000.f));v.impact=std::min(2.f,v.impact+hit);if(hit>.001f)impacts++;}
  v.slipSpeed=std::clamp(d.slipSpeed,0.f,5.f);
  float scrape=v.slipSpeed>.035f?std::clamp(d.scrape,0.f,.7f):0;
  if(scrape>.001f&&v.targetScrape<=.001f){float rate=std::clamp((25.f+150.f*v.slipSpeed)*settings.texture,8.f,1600.f);v.grainWait=(.5f+.5f*noise())*SoundRate/rate;}
  v.targetScrape=scrape;if(v.targetScrape>.001f)scrapeTicks++;
  for(int k=0;k<SoundModes;k++){
   // Contact position changes which modal shapes are driven; higher modes have nodes.
   float projection=.35f+.65f*std::abs(std::cos((k+1)*1.3f*d.x+.73f*(k+2)*d.y+1.11f*d.z));
   if(v.pane)projection*=.45f+.55f*std::cos(std::clamp(d.x,-1.f,1.f)*1.2f)*std::cos(std::clamp(d.y,-1.f,1.f)*1.2f);
   v.m[k].drive=projection;
  }
 }
}
void GlassSynth::render(float*stereo,int frames){
 for(int n=0;n<frames;n++){
  float left=0,right=0;
  for(auto&v:voices){
   if(!v.enabled)continue;
   v.scrape+=(v.targetScrape-v.scrape)*.012f;v.pan+=(v.targetPan-v.pan)*.0008f;
   // A strike starts each mode at its zero crossing. The default 20 ms release
   // loses roughly 40-50 dB; the decay control scales all modal time constants.
   // Compress contact strength gently so tiny taps remain audible at ambient
   // volume, without making the first pile-up disproportionately loud.
   float hit=0;if(v.impact>0){if(v.impactWait>0)v.impactWait--;else{hit=std::sqrt(v.impact)*.24f*settings.impacts;v.impact=0;}}
   float grain=0;
   if(v.targetScrape>.001f&&v.scrape>.001f&&settings.scraping>0){
    if(v.grainWait<=0){
     float rate=std::clamp((25.f+150.f*v.slipSpeed)*settings.texture,8.f,1600.f);
     v.grainWait=SoundRate/rate*(1.f+.72f*noise());
     // Irregular asperity contacts: weak, signed impulses, not a noise bed.
     grain=.12f*v.scrape*noise()*settings.scraping;scrapeGrains++;
    }
    v.grainWait-=1;
   }else v.grainWait=std::max(v.grainWait,1.f);
   float sum=0;float load=1-v.pressure*.00015f;
   for(int k=0;k<SoundModes;k++){auto&m=v.m[k];m.im+=(hit+grain*(.7f+.15f*k))*m.drive;float re=(m.re*m.rc-m.im*m.rs)*load;float im=(m.re*m.rs+m.im*m.rc)*load;m.re=std::abs(re)<1e-18f?0:re;m.im=std::abs(im)<1e-18f?0:im;sum+=m.re*m.gain;}
   if(v.pane)sum*=wallNow;
   float pan=std::clamp(v.pan,-.75f,.75f);left+=sum*(.7071f-.36f*pan);right+=sum*(.7071f+.36f*pan);
  }
  // Gentle high-frequency rolloff, DC removal, smoothed gain, then 5x the
  // previous output gain. A linked lookahead limiter preserves wave shapes
  // instead of saturating every peak through a tanh waveshaper.
  lowL+=.72f*(left-lowL);lowR+=.72f*(right-lowR);dcL+=.012f*(lowL-dcL);dcR+=.012f*(lowR-dcR);volumeNow+=((settings.enabled?settings.volume:0)-volumeNow)*.001f;wallNow+=(settings.walls-wallNow)*.001f;
  float l=(lowL-dcL)*volumeNow*10.5f,r=(lowR-dcR)*volumeNow*10.5f,p=std::max(std::abs(l),std::abs(r));maximumRaw=std::max(maximumRaw,p);
  constexpr int capacity=SoundLookahead+2;
  while(peakCount&&peakQueue[peakHead].sample+SoundLookahead<audioSample){peakHead=(peakHead+1)%capacity;peakCount--;}
  while(peakCount&&peakQueue[(peakHead+peakCount-1)%capacity].peak<=p)peakCount--;
  peakQueue[(peakHead+peakCount)%capacity]={audioSample++,p};peakCount++;
  float target=std::min(1.f,.85f/std::max(.85f,peakQueue[peakHead].peak));
  limiterGain=std::min(target,limiterGain+(1-limiterGain)*.00052f); // ~40 ms release.
  auto delayed=lookahead[delayCursor];lookahead[delayCursor]={l,r};delayCursor=(delayCursor+1)%SoundLookahead;
  stereo[n*2]=delayed[0]*limiterGain;stereo[n*2+1]=delayed[1]*limiterGain;
 }
}
