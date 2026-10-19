#include "scene.h"
#include <random>
#include <limits>
#include <stdexcept>

static btVector3 bt(V3 v){return {v.x,v.y,v.z};}
static V3 v3(const btVector3&v){return {v.x(),v.y(),v.z()};}
static void tri(std::vector<Triangle>&out,V3 a,V3 b,V3 c,int mat,int cell=0){
 if(dot(cross(b-a,c-a),cross(b-a,c-a))<1e-24f)return;
 Triangle t;t.a=F4(a);t.b=F4(b);t.c=F4(c);t.n=F4(norm(cross(b-a,c-a)));t.meta={uint32_t(mat),uint32_t(cell),0,0};out.push_back(t);
}
static void quad(std::vector<Triangle>&o,V3 a,V3 b,V3 c,V3 d,int m){tri(o,a,b,c,m);tri(o,a,c,d,m);}
static void box(std::vector<Triangle>&o,V3 lo,V3 hi,int m){
 V3 a{lo.x,lo.y,lo.z},b{hi.x,lo.y,lo.z},c{hi.x,hi.y,lo.z},d{lo.x,hi.y,lo.z},e{lo.x,lo.y,hi.z},f{hi.x,lo.y,hi.z},g{hi.x,hi.y,hi.z},h{lo.x,hi.y,hi.z};
 quad(o,a,d,c,b,m);quad(o,e,f,g,h,m);quad(o,a,e,h,d,m);quad(o,b,c,g,f,m);quad(o,d,h,g,c,m);quad(o,a,b,f,e,m);
}
static bool near(V3 a,V3 b,float e=2e-5f){return dot(a-b,a-b)<e*e;}
static void unique(std::vector<V3>&p,V3 q){for(auto v:p)if(near(v,q))return;p.push_back(q);}
static std::vector<Face> block(float width,float height,float depth){
 std::vector<Triangle>t;box(t,{-width*.5f,0,-depth*.5f},{width*.5f,height,depth*.5f},0);std::vector<Face>f;
 for(size_t i=0;i<t.size();i+=2){Face a;a.vertices={t[i].a.xyz(),t[i].b.xyz(),t[i].c.xyz(),t[i+1].c.xyz()};a.normal=t[i].n.xyz();a.distance=dot(a.normal,a.vertices[0]);f.push_back(a);}return f;
}
// Clip convex polygon faces and retain the neighboring Voronoi cell for bonding.
static void clip(std::vector<Face>&faces,V3 n,float d,int neighbor=-1){
 std::vector<Face>result;std::vector<V3>cap;bool outside=false,inside=false;
 for(auto&f:faces)for(auto p:f.vertices){float v=dot(n,p)-d;outside|=v>1e-6f;inside|=v< -1e-6f;}
 if(!outside)return;if(!inside){faces.clear();return;}
 for(auto&f:faces){Face out=f;out.vertices.clear();
  for(size_t i=0;i<f.vertices.size();i++){V3 a=f.vertices[i],b=f.vertices[(i+1)%f.vertices.size()];float da=dot(n,a)-d,db=dot(n,b)-d;bool ia=da<=0,ib=db<=0;
   if(ia)unique(out.vertices,a);if(ia!=ib){V3 p=a+(b-a)*(da/(da-db));unique(out.vertices,p);unique(cap,p);}
  }if(out.vertices.size()>=3)result.push_back(std::move(out));
 }
 if(cap.size()>=3){V3 center{};for(auto p:cap)center=center+p;center=center/float(cap.size());V3 u=norm(cross(std::abs(n.z)<.9f?V3{0,0,1}:V3{0,1,0},n)),v=cross(n,u);
  std::sort(cap.begin(),cap.end(),[&](V3 a,V3 b){a=a-center;b=b-center;return std::atan2(dot(a,v),dot(a,u))<std::atan2(dot(b,v),dot(b,u));});result.push_back({cap,n,d,neighbor});
 }
 // Adjacent faces can compute the same intersection in opposite edge order.
 // Reuse the cap's canonical vertices so tiny bevels cannot leave hairline holes.
 for(auto&f:result){std::vector<V3>vertices;for(auto p:f.vertices){for(auto q:cap)if(near(p,q)){p=q;break;}unique(vertices,p);}f.vertices=std::move(vertices);}
 result.erase(std::remove_if(result.begin(),result.end(),[](const Face&f){return f.vertices.size()<3;}),result.end());faces=std::move(result);
}
static float faceArea(const Face&f){float area=0;for(size_t k=1;k+1<f.vertices.size();k++){auto c=cross(f.vertices[k]-f.vertices[0],f.vertices[k+1]-f.vertices[0]);area+=std::sqrt(dot(c,c))*.5f;}return area;}
// Convex source solids share the same plane clipping, bevel and collision path.
// The ellipsoid uses spherical support planes (a deliberately faceted cut).
static std::vector<Face> sourceSolid(int shape,float width,float height,float depth){
 auto faces=block(width,height,depth);V3 center{0,height*.5f,0};
 auto cut=[&](V3 n,float d){V3 scaled{n.x/(width*.5f),n.y/(height*.5f),n.z/(depth*.5f)};float len=std::sqrt(dot(scaled,scaled));clip(faces,scaled/len,(d+dot(scaled,center))/len);};
 if(shape==1){
  // Latitude rings with staggered azimuths, plus exact axial planes. All points
  // lie on the unit sphere, giving convex tangent facets with bounded extents.
  for(int ring=1;ring<8;ring++){float latitude=3.141592654f*ring/8;for(int side=0;side<16;side++){float azimuth=6.283185307f*(side+.5f*(ring%2))/16;cut({std::sin(latitude)*std::cos(azimuth),std::cos(latitude),std::sin(latitude)*std::sin(azimuth)},1);}}
 }else if(shape==2){
  for(int side=0;side<32;side++){float angle=6.283185307f*side/32;cut({std::cos(angle),0,std::sin(angle)},1);}
 }else if(shape==3){
  for(int x:{-1,1})for(int y:{-1,1})for(int z:{-1,1})cut({float(x),float(y),float(z)},1);
 }return faces;
}
static Material material(V3 c,int type,float rough,float ior=1.5f,float emission=0,V3 absorption={},float dispersion=0){return {F4(c,float(type)),F4(rough,ior,emission,0),F4(absorption,dispersion),F4(type==2?1.f:0.f,type==2?.045f:0.f,0,0)};}

CrystalScene::~CrystalScene(){clearPhysics();}
void CrystalScene::clearPhysics(){if(world){for(int i=world->getNumConstraints()-1;i>=0;i--)world->removeConstraint(world->getConstraint(i));for(int i=world->getNumCollisionObjects()-1;i>=0;i--)world->removeCollisionObject(world->getCollisionObjectArray()[i]);}boxWalls.clear();wallLocal.clear();constraints.clear();bodies.clear();shapes.clear();world.reset();solver.reset();broadphase.reset();dispatcher.reset();configuration.reset();}
void CrystalScene::makeStudio(){
 materials.clear();staticTriangles.clear();lights.clear();
 materials.push_back(material({.25f,.26f,.27f},0,.72f));materials.push_back(material({.74f,.55f,.30f},1,.17f));materials.push_back(material({.012f,.016f,.019f},0,.32f));materials.push_back(material({.32f,.34f,.37f},0,.8f));materials.push_back(material({.82f,.82f,.8f},1,.10f));
 quad(staticTriangles,{-200,-.58f,-200},{-200,-.58f,200},{200,-.58f,200},{200,-.58f,-200},0);
 quad(staticTriangles,{-30,-.58f,-6.7f},{30,-.58f,-6.7f},{30,30,-6.7f},{-30,30,-6.7f},3);
 // Twelve chamfered metal rails and eight solid corner nodes, in cube-local space.
 frameTriangles.clear();
 auto metalBox=[&](V3 lo,V3 hi,float bevelWidth,int mat){
  std::vector<Triangle>base;box(base,lo,hi,mat);std::vector<Face>faces;
  for(size_t k=0;k<base.size();k+=2){Face f;f.vertices={base[k].a.xyz(),base[k].b.xyz(),base[k].c.xyz(),base[k+1].c.xyz()};f.normal=base[k].n.xyz();f.distance=dot(f.normal,f.vertices[0]);faces.push_back(f);}
  auto original=faces;
  for(size_t a=0;a<original.size();a++)for(size_t b=a+1;b<original.size();b++)if(std::abs(dot(original[a].normal,original[b].normal))<.01f){auto n=(original[a].normal+original[b].normal)/std::sqrt(2.f);clip(faces,n,(original[a].distance+original[b].distance-bevelWidth)/std::sqrt(2.f));}
  for(auto&f:faces)for(size_t k=1;k+1<f.vertices.size();k++)tri(frameTriangles,f.vertices[0],f.vertices[k],f.vertices[k+1],mat);
 };
 float h=boxHalf+.095f,r=.10f;
 for(int a=-1;a<=1;a+=2)for(int b=-1;b<=1;b+=2){
  metalBox({-h,a*h-r,b*h-r},{h,a*h+r,b*h+r},.035f,4);
  metalBox({a*h-r,-h,b*h-r},{a*h+r,h,b*h+r},.035f,4);
  metalBox({a*h-r,b*h-r,-h},{a*h+r,b*h+r,h},.035f,4);
 }
 for(int x=-1;x<=1;x+=2)for(int y=-1;y<=1;y+=2)for(int z=-1;z<=1;z+=2){V3 p{h*x,h*y,h*z};metalBox(p-V3{.20f,.20f,.20f},p+V3{.20f,.20f,.20f},.07f,1);}
 V3 p{0,17.5f,14.5f},direction=norm(V3{0,6.5f,0}-p),intensity{6400,5900,5100};float radius=.055f;V3 u=norm(cross({0,1,0},direction))*radius,v=norm(cross(direction,u))*radius;
 int mat=int(materials.size());auto lamp=material(intensity/(4*radius*radius),3,0,1,1);lamp.params.w=1;materials.push_back(lamp);quad(staticTriangles,p-u-v,p+u-v,p+u+v,p-u+v,mat);lights.push_back({p,direction,intensity,std::cos(24*.0174532925f),std::cos(34*.0174532925f),radius});
 for(int i=0;i<6;i++){wallMaterials[i]=int(materials.size());materials.push_back(material({.8f,.8f,.8f},0,.8f));}
}
void CrystalScene::meshCell(Cell&cell){
 auto faces=cell.faces;cell.local.clear();cell.hull.clear();struct Cut{V3 n;float d;};std::vector<Cut>cuts;
 float width=std::min(bevel,std::cbrt(cell.volume)*.19f);
 // Equal-distance chamfers on every original edge, independent of camera direction.
 for(size_t i=0;i<faces.size();i++)for(size_t j=i+1;j<faces.size();j++){
  int shared=0;for(auto a:faces[i].vertices)for(auto b:faces[j].vertices)if(near(a,b,8e-5f)){shared++;break;}if(shared<2)continue;
  V3 sum=faces[i].normal+faces[j].normal;float length=std::sqrt(dot(sum,sum));if(length<.01f)continue;
  V3 n=sum/length;float edgeDistance=(faces[i].distance+faces[j].distance)/length;
  float inset=width*std::sqrt(std::max(0.f,(1-dot(faces[i].normal,faces[j].normal))*.5f));
  // Preserve an interior core even for a very thin Voronoi sliver.
  inset=std::min(inset,std::max(0.f,(edgeDistance-dot(n,cell.center))*.45f));cuts.push_back({n,edgeDistance-inset});
 }
 for(auto cut:cuts)if(width>.0001f)clip(faces,cut.n,cut.d);
 const float shrink=.9975f;
 for(auto&face:faces){V3 c{};for(auto p:face.vertices)c=c+p;c=c/float(face.vertices.size());V3 a=(c-cell.center)*shrink;for(size_t i=0;i<face.vertices.size();i++){V3 b=(face.vertices[i]-cell.center)*shrink,d=(face.vertices[(i+1)%face.vertices.size()]-cell.center)*shrink;unique(cell.hull,b);size_t first=cell.local.size();tri(cell.local,a,b,d,cell.material,cell.id+1);if(cell.local.size()!=first)cell.local.back().n=F4(face.normal);}}
}
void CrystalScene::makeCell(Cell&cell){
 meshCell(cell);auto shape=std::make_unique<btConvexHullShape>();for(auto p:cell.hull)shape->addPoint(bt(p),false);shape->recalcLocalAabb();shape->setMargin(.0008f);
 float mass=cell.volume*2.5f;btVector3 inertia;shape->calculateLocalInertia(mass,inertia);btRigidBody::btRigidBodyConstructionInfo ci(mass,nullptr,shape.get(),inertia);ci.m_friction=.62f;ci.m_restitution=.025f;ci.m_linearDamping=.15f;ci.m_angularDamping=.22f;
 float actualHeight=height*objectScale,lift=std::min(1.5f,boxHalf-actualHeight*.5f-.10f);
 auto body=std::make_unique<btRigidBody>(ci);btTransform tr;tr.setIdentity();tr.setOrigin(bt(cell.center+V3{0,boxCenter.y-actualHeight*.5f+lift,0}));body->setWorldTransform(tr);body->setSleepingThresholds(.035f,.035f);body->setCcdMotionThreshold(.15f);body->setCcdSweptSphereRadius(.03f);body->setLinearVelocity({cell.center.x*.10f,0,cell.center.z*.24f});body->setUserIndex(cell.id);cell.body=body.get();world->addRigidBody(body.get());shapes.push_back(std::move(shape));bodies.push_back(std::move(body));
}
void CrystalScene::create(uint32_t seed){
 density=std::clamp(density,1,96);
 objectShape=std::clamp(objectShape,0,3);
 auto dimension=[](float v,float fallback){return std::clamp(std::isfinite(v)?v:fallback,.5f,9.8f);};
 width=dimension(width,8);height=dimension(height,5);depth=dimension(depth,2.8f);
 auto source=sourceSolid(objectShape,width,height,depth);float radius=0;V3 center{0,height*.5f,0};
 for(auto&f:source)for(auto p:f.vertices)radius=std::max(radius,std::sqrt(dot(p-center,p-center)));
 // Keep the entire source inside an inscribed sphere: even the single-cell case
 // can rotate without presenting an impossible fit to the six-wall constraint.
 objectScale=std::min(1.f,(boxHalf-.15f)/radius);for(auto&f:source){for(auto&p:f.vertices)p=p*objectScale;f.distance*=objectScale;}
 sourceSize=V3{width,height,depth}*objectScale;objectVolume=0;for(auto&f:source)for(size_t k=1;k+1<f.vertices.size();k++)objectVolume+=dot(f.vertices[0],cross(f.vertices[k],f.vertices[k+1]))/6;
 clearPhysics();cells.clear();bonds.clear();extracted=0;simulating=true;paused=false;simulationTime=0;boxAngle=0;accumulator=0;revision++;soundGeneration++;soundLastHit.fill(-1000);soundLastSeen.fill(-1000);makeStudio();
 configuration=std::make_unique<btDefaultCollisionConfiguration>();dispatcher=std::make_unique<btCollisionDispatcher>(configuration.get());broadphase=std::make_unique<btDbvtBroadphase>();solver=std::make_unique<btSequentialImpulseConstraintSolver>();world=std::make_unique<btDiscreteDynamicsWorld>(dispatcher.get(),broadphase.get(),solver.get(),configuration.get());world->setGravity({0,-9.81f,0});world->getSolverInfo().m_numIterations=35;world->getSolverInfo().m_splitImpulse=true;
 auto wall=[&](V3 center,V3 half){
  auto shape=std::make_unique<btBoxShape>(bt(half));btRigidBody::btRigidBodyConstructionInfo ci(0,nullptr,shape.get());ci.m_friction=.62f;ci.m_restitution=.04f;auto b=std::make_unique<btRigidBody>(ci);
  btTransform local;local.setIdentity();local.setOrigin(bt(center));auto t=boxTransform()*local;b->setWorldTransform(t);b->setInterpolationWorldTransform(t);
  b->setCollisionFlags((b->getCollisionFlags()&~btCollisionObject::CF_STATIC_OBJECT)|btCollisionObject::CF_KINEMATIC_OBJECT);b->setActivationState(DISABLE_DEACTIVATION);
  b->setUserIndex(96+int(boxWalls.size()));world->addRigidBody(b.get());boxWalls.push_back(b.get());wallLocal.push_back(local);shapes.push_back(std::move(shape));bodies.push_back(std::move(b));
 };
 float h=boxHalf,t=.3f;wall({0,-h-t,0},{h+.6f,t,h+.6f});wall({0,h+t,0},{h+.6f,t,h+.6f});wall({-h-t,0,0},{t,h+.6f,h+.6f});wall({h+t,0,0},{t,h+.6f,h+.6f});wall({0,0,-h-t},{h+.6f,h+.6f,t});wall({0,0,h+t},{h+.6f,h+.6f,t});
 std::mt19937 rng(seed);std::uniform_real_distribution<float>random(0,1);std::vector<V3>seeds;float spacing=std::cbrt(objectVolume/density)*.61f;
 for(int i=0;i<density;i++){V3 p;int accepted=0;bool placed=false;for(int k=0;k<100000;k++){
   p={(-.4925f+.985f*random(rng))*width,(.012f+.976f*random(rng))*height,(-.48f+.96f*random(rng))*depth};p=p*objectScale;
   bool inside=true;for(auto&f:source)if(dot(f.normal,p)>f.distance-1e-5f){inside=false;break;}if(!inside)continue;
   bool good=true;for(auto q:seeds)if(dot(p-q,p-q)<spacing*spacing){good=false;break;}
   if(good||++accepted>=1500){placed=true;break;}
  }if(!placed)throw std::runtime_error("Could not place Voronoi seeds");seeds.push_back(p);}

 for(int i=0;i<density;i++){
  Cell c;c.id=i;c.faces=source;auto a=seeds[i];for(int j=0;j<density;j++)if(j!=i){auto b=seeds[j];float len=std::sqrt(dot(b-a,b-a));clip(c.faces,(b-a)/len,(dot(b,b)-dot(a,a))*.5f/len,j);}
  // Signed tetrahedra give the mass centroid and volume of each closed polyhedron.
  V3 moment{};float volume=0;for(auto&f:c.faces)for(size_t k=1;k+1<f.vertices.size();k++){V3 p=f.vertices[0],q=f.vertices[k],r=f.vertices[k+1];float v=dot(p-a,cross(q-a,r-a))/6;volume+=v;moment=moment+(a+p+q+r)*(v*.25f);}if(volume<1e-5f)throw std::runtime_error("Degenerate Voronoi volume");c.volume=volume;c.center=moment/volume;
  c.colorKey=rng();c.material=int(materials.size());materials.push_back(material({1,1,1},2,0,ior,0,{},.009f));makeCell(c);cells.push_back(std::move(c));
 }
 // Cells are released independently into the larger container; no weld bonds.
 updateMaterials();
}
void CrystalScene::updateOptics(){updateMaterials();}
void CrystalScene::updateMaterials(){
 appearance=sanitizeMaterialSettings(appearance);
 bool contactChanged=false;
 const float friction[]={.62f,.95f,.80f,.55f},bounce[]={.025f,.12f,.018f,.008f};
 const float pigment[]={.20f,.12f,.16f,.27f};
 for(auto&c:cells){auto&m=materials[c.material];uint32_t key=c.colorKey+uint32_t(appearance.colorSeed)*0x9e3779b9u;key^=key>>16;key*=0x7feb352du;key^=key>>15;auto tint=colorPalette(appearance.palette)[key%ColorStopCount];c.tint={std::pow(tint[0],appearance.colorIntensity),std::pow(tint[1],appearance.colorIntensity),std::pow(tint[2],appearance.colorIntensity)};m.absorption={-std::log(c.tint.x)*1.25f,-std::log(c.tint.y)*1.25f,-std::log(c.tint.z)*1.25f,.009f};m.sourceCenter=F4(c.center,pigment[appearance.finish]);V3 color{std::pow(c.tint.x,6.f),std::pow(c.tint.y,6.f),std::pow(c.tint.z,6.f)};m.colorType=F4(color*pigment[appearance.finish],2);m.params.x=1-appearance.shine;m.params.y=ior;
  m.surface={appearance.transparency,appearance.reflectance,float(appearance.finish),appearance.structure};
  contactChanged|=c.body->getFriction()!=friction[appearance.finish]||c.body->getRestitution()!=bounce[appearance.finish];
  c.body->setFriction(friction[appearance.finish]);c.body->setRestitution(bounce[appearance.finish]);
 }
 panelTriangles.clear();float h=boxHalf;
 const float wallFriction[]={.62f,.55f,.85f,.32f},wallBounce[]={.04f,.008f,.018f,.05f};
 for(int i=0;i<6;i++){int style=i<4?appearance.walls[i]:0;auto&m=materials[wallMaterials[i]];
  if(style==0){m=material({1,1,1},2,0,1.5f,0,{.045f,.012f,.025f});m.surface={1,.04f,0,0};m.params.w=float(20+i);}
  else if(style==1)m=material({.80f,.75f,.63f},0,.92f);
  else if(style==2)m=material({.32f,.37f,.40f},0,.84f);
  else m=material({.72f,.76f,.81f},1,.23f);
  if(style){m.surface={0,style==1?.025f:.04f,float(style==1?3:style==2?2:4),.8f};m.params.w=float(10+i);}
  contactChanged|=boxWalls[i]->getFriction()!=wallFriction[style]||boxWalls[i]->getRestitution()!=wallBounce[style];
  boxWalls[i]->setFriction(wallFriction[style]);boxWalls[i]->setRestitution(wallBounce[style]);
  if(!style&&!appearance.glassWalls)continue;
  // Keep a small air gap outside the constraint so flat crystal faces never
  // coincide with a glass interface, even at full contact with the boundary.
  float t=style?.10f:.035f,p=h+(style?0:GlassWallGap);
  if(i==0)box(panelTriangles,{-h,-p-t,-h},{h,-p,h},wallMaterials[i]);
  if(i==1)box(panelTriangles,{-h,p,-h},{h,p+t,h},wallMaterials[i]);
  if(i==2)box(panelTriangles,{-p-t,-h,-h},{-p,h,h},wallMaterials[i]);
  if(i==3)box(panelTriangles,{p,-h,-h},{p+t,h,h},wallMaterials[i]);
  if(i==4)box(panelTriangles,{-h,-h,-p-t},{h,h,-p},wallMaterials[i]);
  if(i==5)box(panelTriangles,{-h,-h,p},{h,h,p+t},wallMaterials[i]);
 }
 // Refresh cached contact friction without moving the existing bodies.
 if(contactChanged&&world)for(auto&body:bodies)if(body->getBroadphaseHandle())world->getBroadphase()->getOverlappingPairCache()->cleanProxyFromPairs(body->getBroadphaseHandle(),dispatcher.get());
 revision++;rebuild();
}
void CrystalScene::updateBevel(){
 for(auto&c:cells)if(!c.removed){meshCell(c);auto shape=std::make_unique<btConvexHullShape>();for(auto p:c.hull)shape->addPoint(bt(p),false);shape->recalcLocalAabb();shape->setMargin(.0008f);auto old=c.body->getCollisionShape();c.body->setCollisionShape(shape.get());btVector3 inertia;shape->calculateLocalInertia(c.volume*2.5f,inertia);c.body->setMassProps(c.volume*2.5f,inertia);c.body->updateInertiaTensor();world->getBroadphase()->getOverlappingPairCache()->cleanProxyFromPairs(c.body->getBroadphaseHandle(),dispatcher.get());world->updateSingleAabb(c.body);for(auto&owned:shapes)if(owned.get()==old){owned=std::move(shape);break;}c.body->activate(true);}revision++;constrainBounds();rebuild();
}
btTransform CrystalScene::boxTransform()const{btTransform t;t.setIdentity();t.setOrigin(bt(boxCenter));t.setRotation(btQuaternion({0,0,1},boxAngle));return t;}
void CrystalScene::updateBox(float dt){
 boxAngle=std::fmod(boxAngle+rotationSpeed*.0174532925f*dt,6.283185307f);auto pose=boxTransform();
 for(size_t i=0;i<boxWalls.size();i++){boxWalls[i]->setWorldTransform(pose*wallLocal[i]);world->updateSingleAabb(boxWalls[i]);}
}
void CrystalScene::rebuild(){
 auto coordinates=[&](Material&m,const btTransform&pose){auto inverse=pose.inverse();auto b=inverse.getBasis();auto o=inverse.getOrigin();m.localX={b[0].x(),b[0].y(),b[0].z(),o.x()};m.localY={b[1].x(),b[1].y(),b[1].z(),o.y()};m.localZ={b[2].x(),b[2].y(),b[2].z(),o.z()};};
 for(int id:wallMaterials)coordinates(materials[id],boxTransform());
 triangles.clear();for(auto&t:staticTriangles)if(!appearance.blackBackground||(t.meta.x!=0&&t.meta.x!=3))triangles.push_back(t);
 auto append=[&](const std::vector<Triangle>&mesh,const btTransform&t){for(auto a:mesh){a.a=F4(v3(t*bt(a.a.xyz())));a.b=F4(v3(t*bt(a.b.xyz())));a.c=F4(v3(t*bt(a.c.xyz())));a.n=F4(v3(t.getBasis()*bt(a.n.xyz())));triangles.push_back(a);}};
 append(frameTriangles,boxTransform());append(panelTriangles,boxTransform());for(auto&c:cells)if(!c.removed){coordinates(materials[c.material],c.body->getWorldTransform());append(c.local,c.body->getWorldTransform());}
}
bool CrystalScene::moving()const{if(!simulating||paused)return false;if(std::abs(rotationSpeed)>.00001f)return true;for(auto&c:cells)if(!c.removed&&c.body->isActive())return true;return false;}
// Contacts enforce six sides; projection prevents residual rotating-vertex drift.
void CrystalScene::constrainBounds(){
 auto pose=boxTransform(),inverse=pose.inverse();btVector3 omega{0,0,rotationSpeed*.0174532925f};
 for(auto&c:cells)if(!c.removed){auto tr=c.body->getWorldTransform();auto local=inverse*tr;V3 lo{1e9f,1e9f,1e9f},hi{-1e9f,-1e9f,-1e9f};for(auto p:c.hull){auto q=v3(local*bt(p));lo={std::min(lo.x,q.x),std::min(lo.y,q.y),std::min(lo.z,q.z)};hi={std::max(hi.x,q.x),std::max(hi.y,q.y),std::max(hi.z,q.z)};}V3 offset{std::max(0.f,-boxHalf-lo.x)+std::min(0.f,boxHalf-hi.x),std::max(0.f,-boxHalf-lo.y)+std::min(0.f,boxHalf-hi.y),std::max(0.f,-boxHalf-lo.z)+std::min(0.f,boxHalf-hi.z)};
  if(dot(offset,offset)>0){tr.setOrigin(tr.getOrigin()+pose.getBasis()*bt(offset));c.body->setWorldTransform(tr);auto wallVelocity=omega.cross(tr.getOrigin()-bt(boxCenter));auto velocity=inverse.getBasis()*(c.body->getLinearVelocity()-wallVelocity);for(int k=0;k<3;k++)if(bt(offset)[k]*velocity[k]<0)velocity[k]=0;c.body->setLinearVelocity(pose.getBasis()*velocity+wallVelocity);world->updateSingleAabb(c.body);}
 }
}
bool CrystalScene::step(float dt){if(!simulating||paused)return false;accumulator+=std::min(dt,.05f);bool changed=false;while(accumulator>=1.f/120){
  bool active=moving();updateBox(1.f/120);if(soundSink)prepareSound();world->stepSimulation(1.f/120,0);simulationTime+=1.f/120;if(soundSink)collectSound();constrainBounds();changed|=active;accumulator-=1.f/120;
 }if(changed)rebuild();return changed;}
void CrystalScene::prepareSound(){
 for(auto&owned:bodies){auto*b=owned.get();int id=b->getUserIndex();if(id<0||id>=SoundVoices)continue;soundOrigin[id]=b->getWorldTransform().getOrigin();soundLinear[id]=b->getLinearVelocity();soundAngular[id]=b->getAngularVelocity();
  if(id>=96){soundAngular[id]={0,0,rotationSpeed*.0174532925f};soundLinear[id]=soundAngular[id].cross(soundOrigin[id]-bt(boxCenter));}
 }
}
void CrystalScene::collectSound(){
 SoundFrame frame;frame.generation=soundGeneration;
 for(int m=0;m<dispatcher->getNumManifolds();m++){
  auto*manifold=dispatcher->getManifoldByIndexInternal(m);auto*a=btRigidBody::upcast(manifold->getBody0());auto*b=btRigidBody::upcast(manifold->getBody1());if(!a||!b)continue;
  int ia=a->getUserIndex(),ib=b->getUserIndex();if(ia<0||ib<0||ia>=SoundVoices||ib>=SoundVoices)continue;
  float impulse=0,closing=0,slide=0;btVector3 pointA{0,0,0},pointB{0,0,0};int contacts=0;
  for(int p=0;p<manifold->getNumContacts();p++){auto&c=manifold->getContactPoint(p);if(c.getDistance()>.003f||c.getAppliedImpulse()<.00001f)continue;
   auto pa=c.getPositionWorldOnA(),pb=c.getPositionWorldOnB(),n=c.m_normalWorldOnB;
   auto beforeA=soundLinear[ia]+soundAngular[ia].cross(pa-soundOrigin[ia]),beforeB=soundLinear[ib]+soundAngular[ib].cross(pb-soundOrigin[ib]);closing=std::max(closing,float(std::max(btScalar(0),-(beforeA-beforeB).dot(n))));
   auto relative=a->getVelocityInLocalPoint(pa-a->getWorldTransform().getOrigin())-b->getVelocityInLocalPoint(pb-b->getWorldTransform().getOrigin());slide=std::max(slide,float((relative-n*relative.dot(n)).length()));impulse+=c.getAppliedImpulse();pointA+=pa;pointB+=pb;contacts++;
  }if(!contacts)continue;pointA/=btScalar(contacts);pointB/=btScalar(contacts);
  int key=std::min(ia,ib)*SoundVoices+std::max(ia,ib);bool fresh=simulationTime-soundLastSeen[key]>.035f;soundLastSeen[key]=simulationTime;
  float hit=0;if(closing>.11f&&(fresh||closing>.36f)&&simulationTime-soundLastHit[key]>.065f){hit=std::min(1.15f,std::log1p(impulse*.12f)*std::sqrt(closing)*.35f);soundLastHit[key]=simulationTime;}
  // Resting support forces alone remain silent; actual slip is required.
  float rubbing=slide>.035f?std::min(.22f,std::sqrt(impulse)*std::min(slide,2.f)*.065f):0;
  auto drive=[&](int id,const btRigidBody*body,btVector3 position){auto&v=frame.voices[id];v.impact=std::min(1.5f,v.impact+hit*(id>=96?.48f:1.f));v.scrape=std::min(.6f,v.scrape+rubbing*(id>=96?.55f:1.f));v.slipSpeed=std::max(v.slipSpeed,slide);v.pressure=std::min(1.f,v.pressure+impulse*.025f);
   auto local=body->getWorldTransform().inverse()*position;float scale=id>=96?boxHalf:std::cbrt(std::max(.05f,cells[id].volume));v.x=std::clamp(local.x()/scale,-1.f,1.f);v.y=std::clamp(local.y()/scale,-1.f,1.f);v.z=std::clamp(local.z()/scale,-1.f,1.f);v.pan=std::clamp(position.x()/9.f,-.7f,.7f);
  };drive(ia,a,pointA);drive(ib,b,pointB);
 }soundSink(frame);
}
bool CrystalScene::extract(int id){if(id<0||id>=int(cells.size())||cells[id].removed)return false;for(auto&b:bonds)if(b.alive&&(b.a==id||b.b==id)){world->removeConstraint(b.joint);b.joint->setEnabled(false);b.alive=false;}world->removeRigidBody(cells[id].body);cells[id].removed=true;extracted++;revision++;simulating=true;paused=false;for(auto&c:cells)if(!c.removed)c.body->activate(true);rebuild();return true;}
int CrystalScene::pick(V3 o,V3 d)const{float best=1e20f;int hit=-1;for(auto&t:triangles){auto tag=materials[t.meta.x].params.w;if(tag>=20&&tag<26)continue;V3 a=t.a.xyz(),e1=t.b.xyz()-a,e2=t.c.xyz()-a,p=cross(d,e2);float det=dot(e1,p);if(std::abs(det)<1e-7f)continue;float inv=1/det;V3 s=o-a;float u=dot(s,p)*inv;if(u<0||u>1)continue;V3 q=cross(s,e1);float v=dot(d,q)*inv;if(v<0||u+v>1)continue;float distance=dot(e2,q)*inv;if(distance>.001f&&distance<best){best=distance;hit=int(t.meta.y)-1;}}return hit;}
float CrystalScene::cohesion()const{if(bonds.empty())return 1;int alive=0;for(auto&b:bonds)alive+=b.alive;return float(alive)/bonds.size();}
bool CrystalScene::boundsValid(float e)const{auto inverse=boxTransform().inverse();for(auto&c:cells)if(!c.removed)for(auto p:c.hull){auto q=v3(inverse*c.body->getWorldTransform()*bt(p));if(std::abs(q.x)>boxHalf+e||std::abs(q.y)>boxHalf+e||std::abs(q.z)>boxHalf+e)return false;}return true;}
bool CrystalScene::geometryValid()const{
 float volume=0;for(auto&c:cells){volume+=c.volume;if(c.local.empty()||c.hull.size()<4)return false;float meshVolume=0;for(auto&t:c.local){if(!std::isfinite(t.n.x)||dot(t.n.xyz(),t.a.xyz())< -1e-4f)return false;meshVolume+=dot(t.a.xyz(),cross(t.b.xyz(),t.c.xyz()))/6;}if(meshVolume<=0||meshVolume>c.volume*1.001f)return false;for(auto&f:c.faces)for(auto p:c.hull)if(dot(f.normal,p+c.center)>f.distance+.001f)return false;
 }return std::abs(volume-objectVolume)<std::max(.001f,objectVolume*.0001f)&&boundsValid();
}
