#include "scene.h"
#include <iostream>
#include <stdexcept>
#include <map>
#include <array>

static void require(bool condition,const char*message){if(!condition)throw std::runtime_error(message);}
static void manifold(const Cell&c){
 using Edge=std::pair<int,int>;std::map<Edge,int>edges;std::vector<V3>vertices;
 auto point=[&](F4 p){for(int i=0;i<int(vertices.size());i++)if(dot(vertices[i]-p.xyz(),vertices[i]-p.xyz())<1e-8f)return i;vertices.push_back(p.xyz());return int(vertices.size())-1;};
 for(auto&t:c.local){int p[]={point(t.a),point(t.b),point(t.c)};if(p[0]==p[1]||p[1]==p[2]||p[0]==p[2])continue;for(int i=0;i<3;i++){auto a=p[i],b=p[(i+1)%3];if(a>b)std::swap(a,b);edges[{a,b}]++;}}
 for(auto&[edge,count]:edges)if(count!=2){std::cerr<<"Cell "<<c.id<<", edge "<<edge.first<<","<<edge.second<<", incidence "<<count<<"\n";require(false,"A beveled crystal is not a closed manifold");}
}
int main(){try{
 for(int test=0;test<4;test++){
  CrystalScene s;s.density=test==3?96:56;s.bevel=test==0?0:test==1?.075f:.18f;s.create(73192+test);require(s.geometryValid(),"Voronoi volume/orientation validation failed");for(auto&c:s.cells)manifold(c);
  float zmin=10,zmax=-10;for(auto&c:s.cells){zmin=std::min(zmin,c.center.z);zmax=std::max(zmax,c.center.z);}require(zmax-zmin>1,"Seeds are not volumetric");
  auto before=s.cells[0].body->getWorldTransform();s.ior=2.3f;s.updateOptics();for(auto&c:s.cells)require(std::abs(s.materials[c.material].params.y-2.3f)<1e-5f,"IOR update failed");
  s.bevel=.12f;s.updateBevel();require(s.geometryValid(),"Bevel update failed");require((s.cells[0].body->getWorldTransform().getOrigin()-before.getOrigin()).length()<1e-4f,"Bevel changed cell pose");
  require(s.boxWalls.size()==6&&s.bonds.empty(),"Cube must have six walls and released crystals");require(s.frameTriangles.size()>100,"Missing metal cube geometry");
  s.rotationSpeed=test==1?-6:test==2?12:4;float initialY=0;for(auto&c:s.cells)initialY+=c.body->getWorldTransform().getOrigin().y();
  float zMotion=0,tilt=0;int frames=test==0?14400:1800;
  for(int frame=0;frame<frames;frame++){s.step(1.f/120);require(s.boundsValid(.002f),"3D containment failed in rotating cube");for(auto&c:s.cells)if(!c.removed){auto t=c.body->getWorldTransform();zMotion=std::max(zMotion,std::abs(t.getOrigin().z()-c.center.z));auto q=t.getRotation();tilt=std::max(tilt,std::abs(q.x())+std::abs(q.y()));}
   if(frame==120){float y=0;for(auto&c:s.cells)y+=c.body->getWorldTransform().getOrigin().y();require(y<initialY-float(s.cells.size()),"Crystals did not fall into the larger cube");}
   if(frame%120==0){auto pose=s.boxTransform();auto axis=pose.getBasis()*btVector3{0,0,1};require((axis-btVector3{0,0,1}).length()<1e-5f,"Cube rotation axis changed");for(auto&t:s.frameTriangles)for(auto p:{t.a,t.b,t.c})require((pose*btVector3{p.x,p.y,p.z}).y()>-.58f,"Rotating metal hits studio floor");}
  }
  require(zMotion>.005f&&tilt>.005f,"Bodies did not translate and rotate in 3D");s.paused=true;auto t=s.cells.back().body->getWorldTransform();require(!s.step(.02f),"Pause did not stop stepping");
  float angle=s.boxAngle;s.step(.04f);require(s.boxAngle==angle,"Pause did not stop cube rotation");
  std::cout<<"Case "<<test<<": PASS; cells "<<s.density<<"; seconds "<<frames/120<<"; speed "<<s.rotationSpeed<<"; z motion "<<zMotion<<"; 3D tilt "<<tilt<<"\n";
 }
 std::cout<<"PASS: closed bevels, cube boundaries, initial fall, dispersion, front-to-back rotation, floor clearance and pause.\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}}
