#pragma once
#include "scene.h"

// OpenXR and the scene are right handed: +Y up, -Z forward. Only the scale,
// horizontal heading and origin change. Never scale the runtime's submitted poses.
struct VrPose { F4 orientation{0,0,0,1}; V3 position; };
inline V3 rotateQuaternion(F4 q,V3 v){V3 t=cross(q.xyz(),v)*2;return v+t*q.w+cross(q.xyz(),t);}
inline V3 rotateY(V3 v,float angle){float c=std::cos(angle),s=std::sin(angle);return{c*v.x+s*v.z,v.y,-s*v.x+c*v.z};}
struct VrPlacement {
 float unitsPerMeter=10,heading=0;V3 anchor;bool centered=false;
 void recenter(const VrPose&head){anchor=head.position;V3 f=rotateQuaternion(head.orientation,{0,0,-1});if(f.x*f.x+f.z*f.z>.001f)heading=std::atan2(-f.x,-f.z);centered=true;}
 V3 vector(V3 v)const{return rotateY(v,-heading);}
 V3 point(V3 p)const{return V3{0,7.5f+.22f*unitsPerMeter,1.8f*unitsPerMeter}+vector(p-anchor)*unitsPerMeter;}
 Camera camera(const VrPose&pose)const{Camera c;c.tracked=true;c.eye=point(pose.position);c.look=vector(rotateQuaternion(pose.orientation,{0,0,-1}));c.horizontal=vector(rotateQuaternion(pose.orientation,{1,0,0}));c.vertical=vector(rotateQuaternion(pose.orientation,{0,1,0}));c.target=c.eye;return c;}
};
struct VrEye {Camera camera;F4 tangents{-1,1,-1,1};}; // left, right, down, up
struct VrPointer {V3 origin,direction;float length=0;bool hit=false;};
inline V3 vrRay(const VrEye&e,float u,float v){return norm(e.camera.forward()+e.camera.right()*(e.tangents.x+(e.tangents.y-e.tangents.x)*u)+e.camera.up()*(e.tangents.w+(e.tangents.z-e.tangents.w)*v));}
