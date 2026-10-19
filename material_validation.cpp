#include "scene.h"
#include <iostream>
#include <stdexcept>
#include <limits>

static void require(bool ok,const char*message){if(!ok)throw std::runtime_error(message);}
static V3 localPoint(const Material&m,V3 p){return {dot(m.localX.xyz(),p)+m.localX.w,dot(m.localY.xyz(),p)+m.localY.w,dot(m.localZ.xyz(),p)+m.localZ.w};}
static void near(V3 a,V3 b){require(dot(a-b,a-b)<1e-8f,"Texture coordinates slipped relative to the body");}
int main(){try{
 static_assert(sizeof(Material)==128,"CPU/GPU material layout mismatch");
 for(float yaw:{0.f,.34f,1.3f,1.5707963f,3.14f}){Camera camera;camera.yaw=yaw;auto target=camera.target,origin=camera.origin(),forward=camera.forward();float distance=camera.distance;camera.pan(40,-25,900);auto delta=camera.target-target;
  require(camera.target.z==target.z&&camera.distance==distance,"Camera pan left the XY plane or changed zoom");near(camera.origin()-origin,delta);near(camera.forward(),forward);require(std::isfinite(delta.x)&&std::isfinite(delta.y)&&std::abs(delta.x)<6&&std::abs(delta.y)<6,"Pan was unstable near an edge-on view");
  if(std::abs(camera.right().x)>.2f){float units=2*.345f*distance/900;require(std::abs(dot(delta,camera.right())+40*units)<1e-4f&&std::abs(dot(delta,camera.up())+25*units)<1e-4f,"Pan direction or screen scale was wrong");}
 }
 MaterialSettings invalid;invalid.finish=100;invalid.transparency=-1;invalid.shine=2;invalid.reflectance=std::numeric_limits<float>::quiet_NaN();invalid.walls={-1,10,2,1};auto clean=sanitizeMaterialSettings(invalid);
 require(clean.finish==3&&clean.transparency==0&&clean.shine==1&&clean.reflectance==.045f&&clean.walls[0]==0&&clean.walls[1]==3,"Settings range sanitization failed");
 invalid.colorMode=99;invalid.palette=99;invalid.colorSeed=-1;invalid.colorIntensity=std::numeric_limits<float>::quiet_NaN();invalid.gradientScale=100;invalid.gradientOffset=-1;invalid.gradientYaw=999;invalid.gradientTilt=-999;invalid.ambientBrightness=999;clean=sanitizeMaterialSettings(invalid);
 require(clean.colorMode==4&&clean.palette==6&&clean.colorSeed==0&&clean.colorIntensity==1&&clean.gradientScale==4&&clean.gradientOffset==0&&clean.gradientYaw==180&&clean.gradientTilt==-90&&clean.ambientBrightness==8,"Color / ambient ranges were not sanitized");
 auto file=std::filesystem::temp_directory_path()/"CrystalMaterialValidation.ini";MaterialSettings saved;saved.preset(2);saved.transparency=.37f;saved.reflectance=.28f;saved.walls={1,2,3,0};saved.glassWalls=true;saved.blackBackground=true;saved.colorMode=3;saved.palette=6;saved.colorSeed=451;saved.colorIntensity=1.3f;saved.gradientScale=2.3f;saved.gradientOffset=.7f;saved.gradientYaw=-120;saved.gradientTilt=16;saved.ambientLight=false;saved.ambientBrightness=4.6f;require(saveMaterialSettings(file,saved)&&loadMaterialSettings(file)==saved,"Material preferences failed to round-trip");std::filesystem::remove(file);
 CrystalScene scene;scene.create();auto original=describeSound(scene);auto pose=scene.cells[0].body->getWorldTransform();auto generation=scene.soundGeneration;
 auto defaultTint=scene.cells[0].tint;
 for(int palette=0;palette<ColorPaletteCount;palette++){
  for(auto tint:colorPalette(palette))for(float channel:tint)require(channel>0&&channel<=1,"Palette has invalid transmission tint");
  scene.appearance.palette=palette;scene.updateMaterials();auto first=scene.cells[0].tint;scene.updateMaterials();near(scene.cells[0].tint,first);
  for(auto&c:scene.cells){auto&m=scene.materials[c.material];require(m.sourceCenter.w>0,"Crystal missing original-object coloring coordinates");near(m.sourceCenter.xyz(),c.center);require(std::isfinite(m.absorption.x)&&m.absorption.x>=0,"Invalid absorption coefficient");}
 }
 scene.appearance.palette=0;scene.updateMaterials();near(scene.cells[0].tint,defaultTint);std::vector<V3>originalTints;for(auto&c:scene.cells)originalTints.push_back(c.tint);scene.appearance.colorSeed=17;scene.updateMaterials();bool shuffled=false;for(auto&c:scene.cells)if(dot(c.tint-originalTints[c.id],c.tint-originalTints[c.id])>.001f)shuffled=true;require(shuffled,"Shuffle did not assign palette colors");
 scene.appearance.colorIntensity=0;scene.updateMaterials();for(auto&c:scene.cells){near(c.tint,{1,1,1});near(scene.materials[c.material].absorption.xyz(),{});}scene.appearance.colorIntensity=1;
 for(int mode=0;mode<5;mode++){scene.appearance.colorMode=mode;scene.updateMaterials();require(scene.soundGeneration==generation,"Color mode restarted physics");require((scene.cells[0].body->getWorldTransform().getOrigin()-pose.getOrigin()).length()<1e-6f,"Color mode moved a crystal");}
 for(int id:scene.wallMaterials)require(scene.materials[id].sourceCenter.w==0,"Crystal coloring was applied to a wall");
 for(int finish=0;finish<4;finish++){
  scene.appearance.preset(finish);scene.appearance.walls.fill(finish);scene.updateMaterials();
  require(scene.soundGeneration==generation,"Material change restarted the simulation");
  require((scene.cells[0].body->getWorldTransform().getOrigin()-pose.getOrigin()).length()<1e-6f,"Material change moved a crystal");
  require(scene.boxWalls.size()==6,"Material change removed collision boundaries");require(scene.panelTriangles.size()==(finish?48:0),"Wrong number of side-wall triangles");
  for(auto&t:scene.panelTriangles){bool side=false;for(int id:scene.wallMaterials)side|=t.meta.x==id;require(side,"An end wall was made visible");}
  auto audio=describeSound(scene);require(audio.voices[100].frequency==original.voices[100].frequency&&audio.voices[101].decay==original.voices[101].decay,"Invisible end-wall audio changed");
  if(finish)require(audio.voices[0].frequency!=original.voices[0].frequency&&audio.voices[96].decay!=original.voices[96].decay,"Acoustic finishes did not change");
  for(float transparency:{0.f,.5f,1.f}){scene.appearance.transparency=transparency;scene.appearance.shine=.3f;scene.appearance.reflectance=.4f;scene.updateMaterials();for(auto&c:scene.cells){auto&m=scene.materials[c.material];require(m.surface.x==transparency&&std::abs(m.params.x-.7f)<1e-6f&&m.surface.y==.4f,"Material values were not uploaded");}}
 }
 scene.appearance.walls.fill(0);scene.updateMaterials();V3 origin=scene.boxCenter+V3{scene.boxHalf+2,0,0};auto cellOrigin=scene.cells[0].body->getWorldTransform().getOrigin();V3 direction=norm(V3{cellOrigin.x(),cellOrigin.y(),cellOrigin.z()}-origin);
 require(scene.pick(origin,direction)>=0,"Invisible walls blocked crystal picking");scene.appearance.walls[3]=2;scene.updateMaterials();require(scene.pick(origin,direction)==-1,"Picking passed through an opaque wall");
 scene.appearance.walls.fill(0);scene.appearance.glassWalls=true;scene.updateMaterials();require(scene.panelTriangles.size()==72,"Glass checkbox must show all six invisible walls");
 require(scene.pick(origin,direction)>=0,"Glass panes blocked crystal picking");
 auto glassAudio=describeSound(scene);require(glassAudio.voices[96].frequency==original.voices[96].frequency&&glassAudio.voices[101].decay==original.voices[101].decay,"Glass display toggle changed wall acoustics");
 for(int i=0;i<6;i++){int id=scene.wallMaterials[i];auto&m=scene.materials[id];require(m.colorType.w==2&&m.surface.x==1&&m.params.y==1.5f,"Wall is not clear refractive glass");int axis=i<2?1:i<4?0:2;float lo=1e9f,hi=-1e9f,volume=0;
  for(auto&t:scene.panelTriangles)if(t.meta.x==id){volume+=dot(t.a.xyz(),cross(t.b.xyz(),t.c.xyz()))/6;for(auto p:{t.a,t.b,t.c}){float v=axis==0?p.x:axis==1?p.y:p.z;lo=std::min(lo,v);hi=std::max(hi,v);}}
  require(std::abs(hi-lo-.035f)<1e-5f&&volume>0,"Glass pane must be a closed thin solid");
  require(std::abs((i%2?lo:-hi)-scene.boxHalf-CrystalScene::GlassWallGap)<1e-5f,"Glass pane must have an air gap outside the collision boundary");
 }
 require((scene.cells[0].body->getWorldTransform().getOrigin()-pose.getOrigin()).length()<1e-6f&&scene.boxWalls.size()==6,"Glass toggle changed physics");
 auto triangleCount=scene.triangles.size();scene.appearance.blackBackground=true;scene.updateMaterials();
 require(scene.triangles.size()+4==triangleCount,"Black mode must remove both floor and backdrop");
 for(auto&t:scene.triangles)require(t.meta.x!=0&&t.meta.x!=3,"Studio geometry remained in black mode");
 require((scene.cells[0].body->getWorldTransform().getOrigin()-pose.getOrigin()).length()<1e-6f&&scene.soundGeneration==generation,"Black mode reset the simulation");
 scene.appearance.blackBackground=false;scene.updateMaterials();require(scene.triangles.size()==triangleCount,"Studio geometry did not return");
 scene.appearance.walls={1,2,3,0};scene.updateMaterials();require(scene.panelTriangles.size()==72,"Mixed finishes must retain the glass end and invisible side walls");require(scene.materials[scene.wallMaterials[0]].colorType.w==0&&scene.materials[scene.wallMaterials[2]].colorType.w==1,"Glass checkbox replaced a solid wall finish");
 scene.appearance.glassWalls=false;scene.updateMaterials();require(scene.panelTriangles.size()==36,"Turning glass off did not hide the panes");
 scene.appearance.walls={1,2,3,0};scene.updateMaterials();require(scene.panelTriangles.size()==36,"Mixed wall visibility failed");
 require(scene.boxWalls[0]->getFriction()!=scene.boxWalls[1]->getFriction()&&scene.boxWalls[1]->getFriction()!=scene.boxWalls[2]->getFriction(),"Wall friction did not change");
 scene.appearance.glassWalls=true;scene.updateMaterials();for(int i=0;i<1200;i++){scene.step(1.f/120);require(scene.boundsValid(.003f),"Containment failed with material walls");}
 auto toV=[](btVector3 v){return V3{v.x(),v.y(),v.z()};};V3 q{.3f,.6f,-.4f};
 for(int id:scene.wallMaterials)near(localPoint(scene.materials[id],toV(scene.boxTransform()*btVector3{q.x,q.y,q.z})),q);
 for(auto&c:scene.cells)near(localPoint(scene.materials[c.material],toV(c.body->getWorldTransform()*btVector3{q.x,q.y,q.z})),q);
 // The same original point on a Voronoi cut must map identically from both
 // independently moved pieces, so gradients cannot jump or swim as they tumble.
 for(auto&c:scene.cells)for(auto&face:c.faces)if(face.neighbor>=0){auto&other=scene.cells[face.neighbor];V3 source=face.vertices.front();
  for(auto*cell:{&c,&other}){auto local=source-cell->center;auto world=toV(cell->body->getWorldTransform()*btVector3{local.x,local.y,local.z});auto&m=scene.materials[cell->material];near(localPoint(m,world)+m.sourceCenter.xyz(),source);}
 }
 std::cout<<"PASS: material ranges/persistence, opaque/transmissive endpoints, independent walls, local texture coordinates, friction/acoustics, pose preservation and 10 seconds of rotating containment.\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}}
