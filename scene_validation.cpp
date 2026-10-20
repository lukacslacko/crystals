#include "scene.h"
#include <iostream>
#include <stdexcept>
#include <map>
#include <array>

static void require(bool condition,const char*message){if(!condition)throw std::runtime_error(message);}
static void manifold(const Cell&c){
 using Edge=std::pair<int,int>;std::map<Edge,int>edges;std::vector<V3>vertices;
 auto point=[&](F4 p){for(int i=0;i<int(vertices.size());i++)if(dot(vertices[i]-p.xyz(),vertices[i]-p.xyz())<1e-10f)return i;vertices.push_back(p.xyz());return int(vertices.size())-1;};
 for(auto&t:c.local){int p[]={point(t.a),point(t.b),point(t.c)};if(p[0]==p[1]||p[1]==p[2]||p[0]==p[2])continue;for(int i=0;i<3;i++){auto a=p[i],b=p[(i+1)%3];if(a>b)std::swap(a,b);edges[{a,b}]++;}}
 for(auto&[edge,count]:edges)if(count!=2){std::cerr<<"Cell "<<c.id<<", edge "<<edge.first<<","<<edge.second<<", incidence "<<count<<"\n";for(auto&[e,n]:edges)if(n!=2){auto a=vertices[e.first],b=vertices[e.second];std::cerr<<n<<": ("<<a.x<<","<<a.y<<","<<a.z<<") - ("<<b.x<<","<<b.y<<","<<b.z<<")\n";}require(false,"A beveled crystal is not a closed manifold");}
}
static void restingStart(const CrystalScene&s){
 float bottom=1e9f,collisionBottom=1e9f;auto first=s.cells.front().body->getWorldTransform();auto sourceOrigin=first.getOrigin()-first.getBasis()*btVector3{s.cells.front().center.x,s.cells.front().center.y,s.cells.front().center.z};
 for(auto&c:s.cells){auto pose=c.body->getWorldTransform();
  require(c.body->getLinearVelocity().length2()==0&&c.body->getAngularVelocity().length2()==0,"New object received a release impulse");
  require((pose.getOrigin()-pose.getBasis()*btVector3{c.center.x,c.center.y,c.center.z}-sourceOrigin).length()<1e-5f&&std::abs(pose.getRotation().dot(first.getRotation()))>.99999f,"Initial placement separated the assembled cells");
  for(auto p:c.hull){float y=(pose*btVector3{p.x,p.y,p.z}).y();bottom=std::min(bottom,y);collisionBottom=std::min(collisionBottom,y-c.body->getCollisionShape()->getMargin());}
 }
 require(std::abs(collisionBottom-(s.boxCenter.y-s.boxHalf))<1e-5f,"Object does not start resting on the bottom collision wall");
 require(s.boundsValid(.001f),"Resting object starts outside the cube");
 if(s.cells.size()==1&&s.objectShape!=1){bool supportedFace=false;auto&c=s.cells.front();auto pose=c.body->getWorldTransform();
  for(auto&t:c.local){auto n=pose.getBasis()*btVector3{t.n.x,t.n.y,t.n.z};if(n.y()>-.99999f)continue;bool level=true;for(auto p:{t.a,t.b,t.c})level&=std::abs((pose*btVector3{p.x,p.y,p.z}).y()-bottom)<1e-4f;supportedFace|=level;}
  require(supportedFace,"Object does not stand on a horizontal face");
 }
}
int main(){try{
 for(int shape=0;shape<4;shape++)for(int count:{1,2,8,96}){
  CrystalScene s;s.objectShape=shape;s.density=count;
  if(count==2){s.width=9.8f;s.height=9.8f;s.depth=9.8f;} // Fit a deliberately oversized object.
  if(count==8){s.width=2;s.height=8;s.depth=1;} // Tall, thin fragments.
  if(count==96){s.width=.5f;s.height=.5f;s.depth=.5f;s.bevel=.18f;} // Many small cells and a wide bevel.
  s.create(73192+count);require(s.cells.size()==count,"Requested cell count was not created");
  if(!s.geometryValid()){float total=0;for(auto&c:s.cells){total+=c.volume;float mesh=0,minPlane=1e9f,outside=0;for(auto&t:c.local){mesh+=dot(t.a.xyz(),cross(t.b.xyz(),t.c.xyz()))/6;minPlane=std::min(minPlane,dot(t.n.xyz(),t.a.xyz()));}for(auto&f:c.faces)for(auto p:c.hull)outside=std::max(outside,dot(f.normal,p+c.center)-f.distance);if(mesh<=0||mesh>c.volume*1.001f||minPlane< -1e-4f||outside>.001f)std::cerr<<"Invalid cell "<<c.id<<": volume "<<c.volume<<", mesh "<<mesh<<", plane "<<minPlane<<", outside "<<outside<<"\n";}std::cerr<<"Total volume "<<total<<" / "<<s.objectVolume<<"; bounds "<<s.boundsValid()<<"\n";}
  require(s.geometryValid(),"Source shape / Voronoi volume failed");
  restingStart(s);
  for(auto&c:s.cells)manifold(c);
  if(shape==0)require(std::abs(s.objectVolume-s.width*s.height*s.depth*std::pow(s.objectScale,3))<.001f,"Box dimensions were not applied");
  if(shape==3)require(std::abs(s.objectVolume-s.width*s.height*s.depth*std::pow(s.objectScale,3)/6)<.001f,"Octahedron dimensions were not applied");
  float initialY=s.cells[0].body->getWorldTransform().getOrigin().y();
  if(count==1){s.rotationSpeed=0;for(int frame=0;frame<120;frame++)s.step(1.f/120);require(s.cells[0].body->getWorldTransform().getOrigin().y()>initialY-.025f,"Single object fell instead of starting supported");s.rotationSpeed=4;}
  int frames=count==1?12000:count==2?1800:240;
  for(int frame=0;frame<frames;frame++){s.step(1.f/120);require(s.boundsValid(.003f),"Containment failed for source shape / low cell count");}
  if(count==1){require(s.extract(0)&&s.extracted==1,"Could not extract the final crystal");s.step(.02f);require(s.boundsValid(),"Empty cube containment failed");}
  std::cout<<"Shape "<<shape<<", cells "<<count<<": PASS; volume "<<s.objectVolume<<"; seconds "<<frames/120<<"\n";
 }
 for(int test=0;test<4;test++){
  CrystalScene s;s.density=test==3?96:56;s.bevel=test==0?0:test==1?.075f:.18f;s.create(73192+test);require(s.geometryValid(),"Voronoi volume/orientation validation failed");restingStart(s);for(auto&c:s.cells)manifold(c);
  float zmin=10,zmax=-10;for(auto&c:s.cells){zmin=std::min(zmin,c.center.z);zmax=std::max(zmax,c.center.z);}require(zmax-zmin>1,"Seeds are not volumetric");
  auto before=s.cells[0].body->getWorldTransform();s.ior=2.3f;s.updateOptics();for(auto&c:s.cells)require(std::abs(s.materials[c.material].params.y-2.3f)<1e-5f,"IOR update failed");
  s.bevel=.12f;s.updateBevel();require(s.geometryValid(),"Bevel update failed");require((s.cells[0].body->getWorldTransform().getOrigin()-before.getOrigin()).length()<1e-4f,"Bevel changed cell pose");
  require(s.boxWalls.size()==6&&s.bonds.empty(),"Cube must have six walls and released crystals");require(s.frameTriangles.size()>100,"Missing metal cube geometry");
  s.rotationSpeed=test==1?-6:test==2?12:4;
  float zMotion=0,tilt=0;int frames=test==0?14400:1800;
  for(int frame=0;frame<frames;frame++){s.step(1.f/120);require(s.boundsValid(.002f),"3D containment failed in rotating cube");for(auto&c:s.cells)if(!c.removed){auto t=c.body->getWorldTransform();zMotion=std::max(zMotion,std::abs(t.getOrigin().z()-c.center.z));auto q=t.getRotation();tilt=std::max(tilt,std::abs(q.x())+std::abs(q.y()));}
   if(frame%120==0){auto pose=s.boxTransform();auto axis=pose.getBasis()*btVector3{0,0,1};require((axis-btVector3{0,0,1}).length()<1e-5f,"Cube rotation axis changed");for(auto&t:s.frameTriangles)for(auto p:{t.a,t.b,t.c})require((pose*btVector3{p.x,p.y,p.z}).y()>-.58f,"Rotating metal hits studio floor");}
  }
  require(zMotion>.005f&&tilt>.005f,"Bodies did not translate and rotate in 3D");s.paused=true;auto t=s.cells.back().body->getWorldTransform();require(!s.step(.02f),"Pause did not stop stepping");
  float angle=s.boxAngle;s.step(.04f);require(s.boxAngle==angle,"Pause did not stop cube rotation");
  std::cout<<"Case "<<test<<": PASS; cells "<<s.density<<"; seconds "<<frames/120<<"; speed "<<s.rotationSpeed<<"; z motion "<<zMotion<<"; 3D tilt "<<tilt<<"\n";
 }
 std::cout<<"PASS: closed bevels, supported start on the bottom wall, face orientation, zero release impulse, cube boundaries, dispersion, front-to-back rotation, floor clearance and pause.\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL: "<<e.what()<<"\n";return 1;}}
