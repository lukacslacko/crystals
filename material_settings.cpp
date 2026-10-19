#include "material_settings.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>

void MaterialSettings::preset(int index){
 finish=std::clamp(index,0,3);transparency=finish==0?1.f:0.f;structure=.65f;
 const float shines[]={1,.20f,.15f,.04f},reflections[]={.045f,.035f,.04f,.025f};
 shine=shines[finish];reflectance=reflections[finish];
}
MaterialSettings sanitizeMaterialSettings(MaterialSettings s){
 auto unit=[](float v,float fallback){return std::isfinite(v)?std::clamp(v,0.f,1.f):fallback;};
 s.finish=std::clamp(s.finish,0,3);s.transparency=unit(s.transparency,1);s.shine=unit(s.shine,1);s.reflectance=unit(s.reflectance,.045f);s.structure=unit(s.structure,.65f);
 for(auto&w:s.walls)w=std::clamp(w,0,3);return s;
}
MaterialSettings loadMaterialSettings(const std::filesystem::path&path){
 MaterialSettings s;std::ifstream in(path);std::string line;
 while(std::getline(in,line)){std::istringstream row(line);std::string key;float v;if(!(row>>key>>v)||!std::isfinite(v))continue;
  if(key=="finish")s.finish=int(std::clamp(v,0.f,3.f));else if(key=="transparency")s.transparency=v;else if(key=="shine")s.shine=v;else if(key=="reflectance")s.reflectance=v;else if(key=="structure")s.structure=v;
  else for(int i=0;i<4;i++)if(key=="wall_"+std::to_string(i))s.walls[i]=int(std::clamp(v,0.f,3.f));
 }return sanitizeMaterialSettings(s);
}
bool saveMaterialSettings(const std::filesystem::path&path,const MaterialSettings&input){
 auto s=sanitizeMaterialSettings(input);std::ofstream out(path);if(!out)return false;
 out<<std::setprecision(9)<<"finish "<<s.finish<<"\ntransparency "<<s.transparency<<"\nshine "<<s.shine<<"\nreflectance "<<s.reflectance<<"\nstructure "<<s.structure<<"\n";
 for(int i=0;i<4;i++)out<<"wall_"<<i<<" "<<s.walls[i]<<"\n";return bool(out);
}
