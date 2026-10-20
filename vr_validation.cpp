#include "vr_math.h"
#include <iostream>
#include <stdexcept>
static void require(bool b,const char*message){if(!b)throw std::runtime_error(message);}
static bool near(V3 a,V3 b,float tolerance=.0001f){return dot(a-b,a-b)<tolerance*tolerance;}
int main(){try{
 VrPlacement p;VrPose center;p.recenter(center);require(near(p.point({0,0,0}),{0,9.7f,18}),"Initial comfortable placement");
 VrPose left,right;left.position.x=-.032f;right.position.x=.032f;auto l=p.camera(left),r=p.camera(right);require(near(r.origin()-l.origin(),{.64f,0,0}),"Headset IPD scales uniformly");require(near(l.forward(),{0,0,-1}),"Right handed forward axis");require(near(l.right(),{1,0,0}),"Left and right must not be swapped");
 center.position={2,1.65f,-4};float turn=1.2f;center.orientation={0,std::sin(turn/2),0,std::cos(turn/2)};p.recenter(center);auto c=p.camera(center);require(near(c.origin(),{0,9.7f,18}),"Recenter position");require(near(c.forward(),{0,0,-1}),"Recenter heading");
 center.position=center.position+rotateY({.1f,0,0},turn);require(near(p.camera(center).origin()-c.origin(),{1,0,0}),"Physical lean remains one to one");
 p.recenter({});VrPose roll;roll.orientation={0,0,std::sin(.25f),std::cos(.25f)};auto rolled=p.camera(roll);require(near(rolled.right(),{std::cos(.5f),std::sin(.5f),0}),"Head roll preserved");require(std::abs(dot(rolled.up(),rolled.right()))<.0001f,"Orthonormal eye basis");
 VrEye eye{p.camera({}),{-.8f,1.2f,-1.1f,.9f}};require(near(vrRay(eye,0,0),norm({-.8f,.9f,-1})),"Asymmetric top left frustum");require(near(vrRay(eye,1,1),norm({1.2f,-1.1f,-1})),"Asymmetric bottom right frustum");require(near(vrRay(eye,.4f,.45f),{0,0,-1}),"Optical center need not be image center");
 // At equal world depth the left eye must see an object farther right than the
 // right eye. This catches a reversed stereo baseline without needing a headset.
 V3 object{0,9.7f,0};float lx=dot(object-l.origin(),l.right())/dot(object-l.origin(),l.forward());float rx=dot(object-r.origin(),r.right())/dot(object-r.origin(),r.forward());require(lx>rx,"Positive stereo disparity");
 std::cout<<"VR placement, IPD, lean, yaw, roll, asymmetric frusta and stereo disparity: PASS\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 1;}}
