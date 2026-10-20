#pragma once
#include <btBulletDynamicsCommon.h>
#include <BulletDynamics/ConstraintSolver/btFixedConstraint.h>
#include <vector>
#include <memory>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include <functional>
#include "sound_model.h"
#include "material_settings.h"

struct V2 { float x=0,y=0; V2 operator+(V2 b)const{return{x+b.x,y+b.y};} V2 operator-(V2 b)const{return{x-b.x,y-b.y};} V2 operator*(float s)const{return{x*s,y*s};} };
struct V3 { float x=0,y=0,z=0; V3 operator+(V3 b)const{return{x+b.x,y+b.y,z+b.z};} V3 operator-(V3 b)const{return{x-b.x,y-b.y,z-b.z};} V3 operator*(float s)const{return{x*s,y*s,z*s};} V3 operator/(float s)const{return{x/s,y/s,z/s};} };
inline float dot(V3 a,V3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
inline V3 cross(V3 a,V3 b){return{a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
inline V3 norm(V3 a){return a/std::max(1e-20f,std::sqrt(dot(a,a)));}
struct F4 {float x=0,y=0,z=0,w=0; F4()=default;F4(float a,float b,float c,float d=0):x(a),y(b),z(c),w(d){} F4(V3 v,float d=0):x(v.x),y(v.y),z(v.z),w(d){} V3 xyz()const{return{x,y,z};}};
struct U4 {uint32_t x=0,y=0,z=0,w=0;};
struct Triangle {F4 a,b,c,n;U4 meta;};
struct Material {F4 colorType,params,absorption,surface;F4 localX{1,0,0,0},localY{0,1,0,0},localZ{0,0,1,0};F4 sourceCenter;};
struct SpotLight {V3 position,direction,intensity;float innerCos,outerCos,radius;};
struct Face {std::vector<V3> vertices;V3 normal;float distance=0;int neighbor=-1;};
struct Cell {int id=0,material=0;uint32_t colorKey=0;std::vector<Face> faces;std::vector<Triangle> local;std::vector<V3> hull;V3 center,tint;float volume=0;btRigidBody* body=nullptr;bool removed=false;};
struct Bond {int a,b;btTypedConstraint* joint=nullptr;float area=1;bool alive=true;};
struct Camera {
 float yaw=.34f,pitch=.18f,distance=34.0f,aperture=0;V3 target{0,7.5f,0};
 V3 origin()const{return target+V3{std::sin(yaw)*std::cos(pitch),std::sin(pitch),std::cos(yaw)*std::cos(pitch)}*distance;}
 V3 forward()const{return norm(target-origin());} V3 right()const{return norm(cross(forward(),{0,1,0}));} V3 up()const{return cross(right(),forward());}
 V3 ray(float x,float y,float aspect)const{return norm(forward()+right()*(x*aspect*.345f)+up()*(y*.345f));}
 void pan(float dx,float dy,float viewportHeight){
  float units=2*.345f*distance/std::max(1.f,viewportHeight);auto r=right(),u=up();
  // Solve the screen projection within world XY. Bound the gain near an edge-on
  // view of that plane; Z and the orbit orientation remain unchanged.
  float x=-dx*units*r.x/std::max(.04f,r.x*r.x),y=(dy*units-u.x*x)/std::max(.25f,u.y);
  target.x+=x;target.y+=y;
 }
};
class CrystalScene {
public:
 static constexpr float GlassWallGap=.015f;
 std::vector<Triangle> triangles,staticTriangles,frameTriangles,panelTriangles;std::vector<Material> materials;std::vector<Cell> cells;std::vector<Bond>bonds;std::vector<SpotLight> lights;
 MaterialSettings appearance;std::array<int,6>wallMaterials{};
 V3 sourceSize{8,5,2.8f}; // Actual source dimensions, frozen until the next reset.
 std::unique_ptr<btDefaultCollisionConfiguration> configuration;
 std::unique_ptr<btCollisionDispatcher> dispatcher;std::unique_ptr<btDbvtBroadphase> broadphase;
 std::unique_ptr<btSequentialImpulseConstraintSolver> solver;std::unique_ptr<btDiscreteDynamicsWorld> world;
 std::vector<std::unique_ptr<btCollisionShape>> shapes;std::vector<std::unique_ptr<btRigidBody>> bodies;std::vector<std::unique_ptr<btTypedConstraint>> constraints;
 int density=56,extracted=0,objectShape=0;float strength=.55f,ior=1.54f,bevel=.075f,width=8,height=5,depth=2.8f,objectVolume=0,objectScale=1;bool simulating=false,paused=false;float simulationTime=0;uint32_t revision=0;
 float boxHalf=5.2f,boxAngle=0,rotationSpeed=4;V3 boxCenter{0,7.5f,0};std::vector<btRigidBody*>boxWalls;std::vector<btTransform>wallLocal;
 uint32_t soundGeneration=0;std::function<void(const SoundFrame&)>soundSink;
 btTransform boxTransform()const;void updateBox(float dt);void updateMaterials();
 ~CrystalScene();void clearPhysics();void create(uint32_t seed=73192);void rebuild();void updateOptics();void updateBevel();bool step(float dt);bool extract(int id);int pick(V3 origin,V3 direction)const;float cohesion()const;bool boundsValid(float tolerance=.045f)const;bool geometryValid()const;bool moving()const;
private:
 float accumulator=0;void makeStudio();void makeCell(Cell&cell);void meshCell(Cell&cell);void placeOnBottom();void setWallVelocities();void constrainBounds();
 std::array<btVector3,SoundVoices>soundLinear,soundAngular,soundOrigin;
 std::array<float,SoundVoices*SoundVoices>soundLastHit{},soundLastSeen{};
 void prepareSound();void collectSound();
};
