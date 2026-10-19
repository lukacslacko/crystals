#include "material_settings.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>

const char* colorPaletteName(int index){
 static constexpr const char*names[]={"Pastel glass","Jewel tones","Ocean","Aurora","Ember","Earth","Spectrum"};return names[std::clamp(index,0,ColorPaletteCount-1)];
}
const ColorPalette& colorPalette(int index){
 // Transmission tints. Opaque pigment uses the sixth power, matching the
 // original crystal palette; the same table feeds CPU materials and the GPU.
 static constexpr ColorPalette palettes[]={
  {{{.76f,.98f,.94f},{.94f,.83f,.99f},{.99f,.94f,.72f},{.72f,.94f,.99f},{.99f,.80f,.88f},{.91f,.99f,.79f},{.88f,.91f,.99f},{.99f,.88f,.73f}}},
  {{{.99f,.58f,.69f},{.97f,.72f,.52f},{.98f,.91f,.55f},{.55f,.96f,.72f},{.51f,.90f,.98f},{.56f,.65f,.99f},{.80f,.55f,.98f},{.98f,.58f,.89f}}},
  {{{.51f,.62f,.83f},{.53f,.71f,.94f},{.54f,.83f,.99f},{.57f,.94f,.98f},{.66f,.98f,.90f},{.77f,.98f,.90f},{.88f,.99f,.97f},{.70f,.84f,.99f}}},
  {{{.61f,.57f,.93f},{.78f,.60f,.99f},{.94f,.64f,.95f},{.76f,.78f,.99f},{.55f,.94f,.98f},{.56f,.99f,.80f},{.82f,.99f,.62f},{.97f,.85f,.79f}}},
  {{{.62f,.40f,.50f},{.82f,.43f,.49f},{.98f,.51f,.43f},{.99f,.68f,.45f},{.99f,.83f,.53f},{.99f,.94f,.71f},{.98f,.75f,.68f},{.86f,.52f,.65f}}},
  {{{.66f,.58f,.47f},{.79f,.65f,.51f},{.92f,.79f,.60f},{.98f,.93f,.78f},{.84f,.89f,.68f},{.65f,.81f,.63f},{.56f,.70f,.65f},{.73f,.75f,.73f}}},
  {{{.99f,.55f,.57f},{.99f,.74f,.49f},{.99f,.98f,.56f},{.62f,.98f,.56f},{.52f,.97f,.96f},{.57f,.70f,.99f},{.78f,.55f,.99f},{.99f,.57f,.88f}}}
 };return palettes[std::clamp(index,0,ColorPaletteCount-1)];
}
void MaterialSettings::preset(int index){
 finish=std::clamp(index,0,3);transparency=finish==0?1.f:0.f;structure=.65f;
 const float shines[]={1,.20f,.15f,.04f},reflections[]={.045f,.035f,.04f,.025f};
 shine=shines[finish];reflectance=reflections[finish];
}
MaterialSettings sanitizeMaterialSettings(MaterialSettings s){
 auto unit=[](float v,float fallback){return std::isfinite(v)?std::clamp(v,0.f,1.f):fallback;};
 s.finish=std::clamp(s.finish,0,3);s.transparency=unit(s.transparency,1);s.shine=unit(s.shine,1);s.reflectance=unit(s.reflectance,.045f);s.structure=unit(s.structure,.65f);
 auto limit=[](float v,float lo,float hi,float fallback){return std::isfinite(v)?std::clamp(v,lo,hi):fallback;};
 s.colorMode=std::clamp(s.colorMode,0,4);s.palette=std::clamp(s.palette,0,ColorPaletteCount-1);s.colorSeed=std::clamp(s.colorSeed,0,1000000);
 s.colorIntensity=limit(s.colorIntensity,0,2,1);s.gradientScale=limit(s.gradientScale,.25f,4,1);s.gradientOffset=limit(s.gradientOffset,0,1,0);
 s.gradientYaw=limit(s.gradientYaw,-180,180,45);s.gradientTilt=limit(s.gradientTilt,-90,90,35.26439f);s.ambientBrightness=limit(s.ambientBrightness,0,8,1);
 for(auto&w:s.walls)w=std::clamp(w,0,3);return s;
}
MaterialSettings loadMaterialSettings(const std::filesystem::path&path){
 MaterialSettings s;std::ifstream in(path);std::string line;
 while(std::getline(in,line)){std::istringstream row(line);std::string key;float v;if(!(row>>key>>v)||!std::isfinite(v))continue;
  if(key=="finish")s.finish=int(std::clamp(v,0.f,3.f));else if(key=="transparency")s.transparency=v;else if(key=="shine")s.shine=v;else if(key=="reflectance")s.reflectance=v;else if(key=="structure")s.structure=v;
  else if(key=="glass_walls")s.glassWalls=v!=0;
  else if(key=="black_background")s.blackBackground=v!=0;
  else if(key=="color_mode")s.colorMode=int(std::clamp(v,0.f,4.f));
  else if(key=="palette")s.palette=int(std::clamp(v,0.f,float(ColorPaletteCount-1)));
  else if(key=="color_seed")s.colorSeed=int(std::clamp(v,0.f,1000000.f));
  else if(key=="color_intensity")s.colorIntensity=v;
  else if(key=="gradient_scale")s.gradientScale=v;
  else if(key=="gradient_offset")s.gradientOffset=v;
  else if(key=="gradient_yaw")s.gradientYaw=v;
  else if(key=="gradient_tilt")s.gradientTilt=v;
  else if(key=="ambient_light")s.ambientLight=v!=0;
  else if(key=="ambient_brightness")s.ambientBrightness=v;
  else for(int i=0;i<4;i++)if(key=="wall_"+std::to_string(i))s.walls[i]=int(std::clamp(v,0.f,3.f));
 }return sanitizeMaterialSettings(s);
}
bool saveMaterialSettings(const std::filesystem::path&path,const MaterialSettings&input){
 auto s=sanitizeMaterialSettings(input);std::ofstream out(path);if(!out)return false;
 out<<std::setprecision(9)<<"finish "<<s.finish<<"\ntransparency "<<s.transparency<<"\nshine "<<s.shine<<"\nreflectance "<<s.reflectance<<"\nstructure "<<s.structure<<"\n";
 for(int i=0;i<4;i++)out<<"wall_"<<i<<" "<<s.walls[i]<<"\n";out<<"glass_walls "<<s.glassWalls<<"\nblack_background "<<s.blackBackground<<"\n";
 out<<"color_mode "<<s.colorMode<<"\npalette "<<s.palette<<"\ncolor_seed "<<s.colorSeed<<"\ncolor_intensity "<<s.colorIntensity
  <<"\ngradient_scale "<<s.gradientScale<<"\ngradient_offset "<<s.gradientOffset<<"\ngradient_yaw "<<s.gradientYaw<<"\ngradient_tilt "<<s.gradientTilt
  <<"\nambient_light "<<s.ambientLight<<"\nambient_brightness "<<s.ambientBrightness<<"\n";return bool(out);
}
