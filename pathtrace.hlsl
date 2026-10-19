struct Surface { float4 a,b,c,n; uint4 meta; };
struct Material { float4 colorType,params,absorption,surface,localX,localY,localZ; };
RaytracingAccelerationStructure Scene : register(t0);
StructuredBuffer<Surface> Triangles : register(t1);
StructuredBuffer<Material> Materials : register(t2);
RWTexture2D<float4> Accumulation : register(u0);
RWTexture2D<float4> Display : register(u1);
RWTexture2D<float4> Guide : register(u2);
RWTexture2DArray<uint> PhotonMap : register(u3);
RWTexture2D<float4> HistoryColor : register(u4);
RWTexture2D<float4> HistoryGuide : register(u5);
RWTexture2D<float4> Filtered : register(u6);
RWTexture2D<float4> DenoiseTemp : register(u7);
RWTexture2D<float4> PhotonTemp : register(u8);
RWTexture2D<float4> PhotonFiltered : register(u9);
RWTexture2D<float4> Resolved : register(u10);
cbuffer Constants : register(b0) {
 float4 CameraOrigin,CameraForward,CameraRight,CameraUp,Settings;
 uint4 Counts,Options;
 float4 LightPosition[4],LightDirection[4],LightColor[4];
 float4 PreviousOrigin,PreviousForward,PreviousRight,PreviousUp,RenderParams,SceneParams;
};
static const float PI=3.14159265359;
static const float PHOTON_SCALE=16384;
uint hash(uint x){x^=x>>16;x*=0x7feb352d;x^=x>>15;x*=0x846ca68b;x^=x>>16;return x;}
float rnd(inout uint seed){seed=hash(seed+0x9e3779b9);return (seed>>8)*(1.0/16777216.0);}
float3 tangent(float3 n){return normalize(cross(abs(n.z)<.9?float3(0,0,1):float3(0,1,0),n));}
float3 basisSample(float3 n,float2 u){float r=sqrt(u.x),phi=2*PI*u.y;float3 t=tangent(n);return normalize(t*(r*cos(phi))+cross(n,t)*(r*sin(phi))+n*sqrt(1-u.x));}
float luminance(float3 c){return dot(c,float3(.2126,.7152,.0722));}
float3 localPoint(Material m,float3 p){return float3(dot(m.localX,float4(p,1)),dot(m.localY,float4(p,1)),dot(m.localZ,float4(p,1)));}
float lattice(int3 p){return (hash(asuint(p.x)*1597334677u^asuint(p.y)*3812015801u^asuint(p.z)*2798796415u)>>8)*(1.0/16777216.0);}
float noise3(float3 p){int3 i=int3(floor(p));float3 f=frac(p);f=f*f*(3-2*f);return lerp(lerp(lerp(lattice(i),lattice(i+int3(1,0,0)),f.x),lerp(lattice(i+int3(0,1,0)),lattice(i+int3(1,1,0)),f.x),f.y),lerp(lerp(lattice(i+int3(0,0,1)),lattice(i+int3(1,0,1)),f.x),lerp(lattice(i+int3(0,1,1)),lattice(i+int3(1,1,1)),f.x),f.y),f.z);}
float heightField(float3 q,int style){
 if(style==1)return noise3(q*24)*.004+noise3(q*7)*.005;
 if(style==2)return noise3(q*7)*.027+noise3(q*31)*.004;
 if(style==3)return noise3(q*float3(65,8,45))*.0025+noise3(q*4)*.004;
 return noise3(q*float3(110,3,72))*.0008;
}
void surfaceDetail(inout Material m,float3 p,float3 geometricNormal,out float3 n){
 n=geometricNormal;int style=int(m.surface.z);float amount=m.surface.w;if(style==0||amount<=0)return;
 float3 q=localPoint(m,p);float h=heightField(q,style),e=.004;
 float3 g=(float3(heightField(q+float3(e,0,0),style),heightField(q+float3(0,e,0),style),heightField(q+float3(0,0,e),style))-h)/e;
 float3 gradient=m.localX.xyz*g.x+m.localY.xyz*g.y+m.localZ.xyz*g.z;
 n=normalize(n-amount*(gradient-n*dot(n,gradient)));
 float grain=noise3(q*(style==2?13:32));float variation=style==2?lerp(.55,1.15,grain):style==3?lerp(.82,1.04,grain):lerp(.91,1.02,grain);
 m.colorType.xyz*=lerp(1,variation,amount);
}
float fresnelSurface(Material m,float cosine){float f=saturate(m.surface.y);return f<=0?0:f+(1-f)*pow(1-saturate(cosine),5);}
float3 attenuation(Material m,float distance){float density=m.colorType.w==2?-log(max(.001,m.surface.x))*.32:0;return exp(-(m.absorption.xyz+density)*distance);}
float3 roughDirection(float3 ideal,float3 side,float rough,float2 sample){
 float3 t=tangent(ideal);float a=sample.y*2*PI,r=sqrt(sample.x)*rough*rough*1.2;
 float3 d=normalize(ideal+r*(t*cos(a)+cross(ideal,t)*sin(a)));return dot(d,side)>.0001?d:ideal;
}
float cone(uint i,float3 direction){return smoothstep(LightDirection[i].w,LightColor[i].w,dot(direction,LightDirection[i].xyz));}
float3 primaryRay(float2 pixel){float2 uv=pixel/float2(Counts.xy)*2-1;return normalize(CameraForward.xyz+CameraRight.xyz*(uv.x*Settings.x*Settings.y)-CameraUp.xyz*(uv.y*Settings.x));}
bool intersect(float3 o,float3 d,float maximum,out uint primitive,out float distance){
 RayDesc ray;ray.Origin=o;ray.Direction=d;ray.TMin=.00015;ray.TMax=maximum;
 RayQuery<RAY_FLAG_FORCE_OPAQUE> query;query.TraceRayInline(Scene,RAY_FLAG_NONE,255,ray);while(query.Proceed()){}
 if(query.CommittedStatus()==COMMITTED_TRIANGLE_HIT){primitive=query.CommittedPrimitiveIndex();distance=query.CommittedRayT();return true;}
 primitive=0;distance=maximum;return false;
}
bool glassWall(Material m){return m.params.w>=20&&m.params.w<26;}
float3 visibility(float3 o,float3 d,float maximum){
 // Thin parallel panes transmit a straight shadow connection with Fresnel loss.
 // Camera and photon rays still trace both refracting faces of the actual solid.
 float3 transmission=1;int medium=-1;
 [loop]for(int step=0;step<24;step++){
  uint primitive;float t;if(!intersect(o,d,maximum,primitive,t))return transmission;
  if(medium>=0)transmission*=attenuation(Materials[medium],t);
  Surface hit=Triangles[primitive];Material m=Materials[hit.meta.x];if(!glassWall(m))return 0;
  float cosine=abs(dot(d,hit.n.xyz));transmission*=1-fresnelSurface(m,cosine);
  medium=dot(d,hit.n.xyz)<0?int(hit.meta.x):-1;o+=d*(t+.0005);maximum-=t+.0005;if(maximum<=0)return transmission;
 }return 0;
}
float3 environment(float3 d){return (Options.w&16)!=0?float3(0,0,0):lerp(float3(.004,.006,.011),float3(.025,.033,.05),smoothstep(-.15,.8,d.y));}
bool receiverUV(float3 p,uint material,out float2 pixel,out float area){
 if(material==3){pixel=float2((p.x+12)/24*2048,(p.y+.58)/22*1024);area=528.0/(2048*1024);return all(pixel>=0)&&pixel.x<2047&&pixel.y<1023;}
 if(material==0){pixel=float2((p.x+12)/24*2048,(p.z+8)/20*1024+1024);area=480.0/(2048*1024);return pixel.x>=0&&pixel.x<2047&&pixel.y>=1024&&pixel.y<2047;}
 Material m=Materials[material];int panel=int(m.params.w)-10;
 if(panel>=0&&panel<4){float3 q=localPoint(m,p);float plane=panel<2?q.y:q.x;
  // Only the inside face receives this tile; do not leak caustics through an opaque panel.
  if(abs(plane-((panel%2)?SceneParams.w:-SceneParams.w))>.003){pixel=0;area=1;return false;}
  float2 uv=(panel<2?q.xz:q.zy)/(2*SceneParams.w)+.5;pixel=uv*1023+float2(2048+(panel%2)*1024,(panel/2)*1024);area=4*SceneParams.w*SceneParams.w/(1024*1024);return all(uv>=0)&&all(uv<1);}
 pixel=0;area=1;return false;
}
int4 photonBounds(int2 p){int x=p.x<2048?0:2048+((p.x-2048)/1024)*1024;int width=p.x<2048?2048:1024;int y=(p.y/1024)*1024;return int4(x,y,x+width-1,y+1023);}
float3 photonAt(int2 q){return float3(PhotonMap[int3(q,0)],PhotonMap[int3(q,1)],PhotonMap[int3(q,2)]);}
float3 causticIrradiance(float3 p,uint material){
 if((Options.w&4)==0)return 0;float2 pixel;float area;if(!receiverUV(p,material,pixel,area))return 0;
 int2 q=int2(pixel);float2 f=frac(pixel);int4 bounds=photonBounds(q);int2 next=min(q+1,bounds.zw);
 float3 a=lerp(PhotonFiltered[q].xyz,PhotonFiltered[int2(next.x,q.y)].xyz,f.x);
 float3 b=lerp(PhotonFiltered[int2(q.x,next.y)].xyz,PhotonFiltered[next].xyz,f.x);
 return lerp(a,b,f.y)/PHOTON_SCALE*RenderParams.z;
}
void deposit(float3 p,uint material,float3 flux){
 float2 pixel;float area;if(!receiverUV(p,material,pixel,area))return;
 int2 base=int2(pixel);float3 energy=max(0,flux/area*PHOTON_SCALE);
 [unroll]for(int y=-2;y<=2;y++)[unroll]for(int x=-2;x<=2;x++){
  int2 q=base+int2(x,y);int4 bounds=photonBounds(base);if(q.x<bounds.x||q.x>bounds.z||q.y<bounds.y||q.y>bounds.w)continue;
  float w=exp(-.5*(x*x+y*y))/6.168924;uint3 e=uint3(min(energy*w,10000000));
  if(e.x)InterlockedAdd(PhotonMap[int3(q,0)],e.x);if(e.y)InterlockedAdd(PhotonMap[int3(q,1)],e.y);if(e.z)InterlockedAdd(PhotonMap[int3(q,2)],e.z);
 }
}
[numthreads(8,8,1)]void ClearPhotons(uint3 id:SV_DispatchThreadID){if(id.x<uint(SceneParams.z>0?4096:2048)&&id.y<2048){PhotonMap[uint3(id.xy,0)]=0;PhotonMap[uint3(id.xy,1)]=0;PhotonMap[uint3(id.xy,2)]=0;}}
[numthreads(256,1,1)]void PhotonTrace(uint3 id:SV_DispatchThreadID){
 uint count=uint(RenderParams.y),index0=id.x+id.y*8388608;if(index0>=count)return;uint triplet=index0/3;uint seed=hash(triplet+37181);uint light=triplet%uint(SceneParams.x);int channel=int(index0%3);
 float3 origin=LightPosition[light].xyz;float3 axis=LightDirection[light].xyz,u=tangent(axis),v=cross(axis,u);
 float angle=2*PI*rnd(seed),radius=sqrt(rnd(seed))*LightPosition[light].w;origin+=(u*cos(angle)+v*sin(angle))*radius;
 // Uniform solid-angle emission covers the front, top and sides of the volume.
 float2 emission=float2((triplet+.5)/ceil(count/3.0),(reversebits(triplet)^0x568af39d)*(1.0/4294967296.0));
 float cosine=lerp(1,LightDirection[light].w,emission.x),azimuth=2*PI*emission.y,sine=sqrt(1-cosine*cosine);
 float3 d=normalize(axis*cosine+(u*cos(azimuth)+v*sin(azimuth))*sine);
 float solidAngle=2*PI*(1-LightDirection[light].w);
 float3 flux=LightColor[light].xyz*(SceneParams.x*solidAngle*cone(light,d)/count);
 if(Options.z!=0)flux*=channel==0?float3(3,0,0):channel==1?float3(0,3,0):float3(0,0,3);else channel=1;
 int medium=-1;bool touchedCrystal=false;
 [loop]for(uint bounce=0;bounce<32;bounce++){
  uint index;float distance;if(!intersect(origin,d,100,index,distance))return;
  Surface hit=Triangles[index];Material m=Materials[hit.meta.x];float3 p=origin+d*distance;
  if(medium>=0)flux*=attenuation(Materials[medium],distance);if(max(flux.x,max(flux.y,flux.z))<1e-10)return;
  bool entering=dot(d,hit.n.xyz)<0;float3 n=entering?hit.n.xyz:-hit.n.xyz;int kind=int(m.colorType.w);
  float3 geometric=n;surfaceDetail(m,p,geometric,n);
  if(kind==2){float ior=m.params.y+(channel==0?-1:channel==2?1:0)*m.absorption.w;float3 tr=refract(d,n,entering?1/ior:ior);float fr=fresnelSurface(m,-dot(d,n));if(dot(tr,tr)<.01)fr=1;float choice=rnd(seed);
   if(choice<fr){d=roughDirection(reflect(d,n),geometric,m.params.x,float2(rnd(seed),rnd(seed)));touchedCrystal=true;}
   else if(choice<fr+(1-fr)*m.surface.x){d=roughDirection(normalize(tr),-geometric,m.params.x,float2(rnd(seed),rnd(seed)));medium=entering?int(hit.meta.x):-1;touchedCrystal=touchedCrystal||!glassWall(m);}
   else return;
  }
  else if(kind==1){d=roughDirection(reflect(d,n),geometric,m.params.x,float2(rnd(seed),rnd(seed)));flux*=m.colorType.xyz;}
  else{if(kind==0&&touchedCrystal)deposit(p,hit.meta.x,flux);return;}
  origin=p+d*.0005;
 }
}
[numthreads(8,8,1)]void PhotonHorizontal(uint3 id:SV_DispatchThreadID){
 if(id.x>=uint(SceneParams.z>0?4096:2048)||id.y>=2048)return;float3 sum=0;float weights=0;int4 bounds=photonBounds(int2(id.xy));
 float variance=RenderParams.y<2000000?24:4.5;int radius=RenderParams.y<2000000?10:4;
 [loop]for(int x=-radius;x<=radius;x++){float w=exp(-float(x*x)/variance);sum+=photonAt(int2(clamp(int(id.x)+x,bounds.x,bounds.z),id.y))*w;weights+=w;}
 PhotonTemp[id.xy]=float4(sum/weights,1);
}
[numthreads(8,8,1)]void PhotonVertical(uint3 id:SV_DispatchThreadID){
 if(id.x>=uint(SceneParams.z>0?4096:2048)||id.y>=2048)return;float3 sum=0;float weights=0;int low=id.y<1024?0:1024;
 float variance=RenderParams.y<2000000?24:4.5;int radius=RenderParams.y<2000000?10:4;
 [loop]for(int y=-radius;y<=radius;y++){float w=exp(-float(y*y)/variance);sum+=PhotonTemp[int2(id.x,clamp(int(id.y)+y,low,low+1023))].xyz*w;weights+=w;}
 PhotonFiltered[id.xy]=float4(sum/weights,1);
}
float3 metalBRDF(float3 n,float3 view,float3 light,float3 f0,float roughness){
 float nv=max(.001,dot(n,view)),nl=max(.001,dot(n,light));float3 h=normalize(view+light);
 float nh=max(0,dot(n,h)),vh=max(0,dot(view,h));float a=max(.025,roughness*roughness),a2=a*a;
 float denominator=nh*nh*(a2-1)+1;float D=a2/(PI*denominator*denominator);float k=(roughness+1)*(roughness+1)/8;
 float G=(nv/(nv*(1-k)+k))*(nl/(nl*(1-k)+k));float3 F=f0+(1-f0)*pow(1-vh,5);return F*D*G/(4*nv*nl);
}
float3 directLighting(float3 p,float3 n,float3 view,Material m,bool metallic){
 float3 sum=0;
 [loop]for(uint i=0;i<uint(SceneParams.x);i++){
  float3 delta=LightPosition[i].xyz-p;float r2=dot(delta,delta),distance=sqrt(r2);float3 l=delta/distance;float nl=max(0,dot(n,l));
  float beam=cone(i,-l);if(nl<=0||beam<=0)continue;float f=fresnelSurface(m,dot(n,view)),transmission=m.colorType.w==2?m.surface.x:0;
  float3 brdf=metallic?metalBRDF(n,view,l,m.colorType.xyz,m.params.x):m.colorType.xyz*((1-f)*(1-transmission)/PI)+(m.surface.y>0?metalBRDF(n,view,l,m.surface.y.xxx,m.params.x):float3(0,0,0));
  sum+=brdf*LightColor[i].xyz*(beam*nl/r2)*visibility(p+n*.0005,l,distance-.002);
 }return sum;
}
struct OpticalRay {float3 o,d,weight;int medium;uint depth,rough;};
void surfaceLobes(Material m,float3 d,float3 n,bool entering,int channel,out float3 reflected,out float3 transmitted,out float3 reflectionWeight,out float transmissionWeight,out float diffuseWeight){
 reflected=reflect(d,n);transmitted=0;float f=fresnelSurface(m,-dot(d,n));float transmission=m.colorType.w==2?m.surface.x:0;
 if(transmission>0){float ior=m.params.y+(channel==0?-1:channel==2?1:0)*m.absorption.w;transmitted=refract(d,n,entering?1/ior:ior);if(dot(transmitted,transmitted)<.01)f=1;else transmitted=normalize(transmitted);}
 reflectionWeight=m.colorType.w==1?m.colorType.xyz+(1-m.colorType.xyz)*pow(1-saturate(-dot(d,n)),5):f.xxx;
 transmissionWeight=(1-f)*transmission;diffuseWeight=m.colorType.w==1?0:(1-f)*(1-transmission);
}
// Bounded Whitted integration: no Russian roulette or random Fresnel decisions.
// Small branches are deliberately truncated; diffuse indirect is an irradiance
// approximation. The stochastic integrator remains separately available.
float3 stablePath(float3 o,float3 d,int channel){
 OpticalRay stack[8];int size=1;stack[0].o=o;stack[0].d=d;stack[0].weight=1;stack[0].medium=-1;stack[0].depth=0;stack[0].rough=0;float3 result=0;
 [loop]for(uint work=0;work<96&&size>0;){
  OpticalRay ray=stack[--size];
  [loop]for(;ray.depth<Options.x&&work<96;ray.depth++){
   work++;uint primitive;float distance;
   if(!intersect(ray.o,ray.d,300,primitive,distance)){result+=ray.weight*environment(ray.d);break;}
   Surface hit=Triangles[primitive];Material m=Materials[hit.meta.x];float3 p=ray.o+ray.d*distance;
   bool entering=dot(ray.d,hit.n.xyz)<0;float3 geometric=entering?hit.n.xyz:-hit.n.xyz,n;surfaceDetail(m,p,geometric,n);
   if(ray.medium>=0)ray.weight*=attenuation(Materials[ray.medium],distance);
   if(max(ray.weight.x,max(ray.weight.y,ray.weight.z))<.006)break;
   int kind=int(m.colorType.w);
   if(kind==3){
    if(ray.depth==0){ray.o=p+ray.d*.001;continue;}
    // Rough-surface direct highlights are already integrated by directLighting.
    // Do not count a rare, extremely bright emitter hit from the cone again.
    if(ray.rough==0){uint lamp=max(0,int(m.params.w)-1);result+=ray.weight*m.colorType.xyz*m.params.z*cone(lamp,-ray.d);}break;
   }
   float3 reflected,transmitted,wr;float wt,wd;surfaceLobes(m,ray.d,n,entering,channel,reflected,transmitted,wr,wt,wd);
   if(dot(reflected,geometric)<=0)reflected=reflect(ray.d,geometric);
   if(wt>0&&dot(transmitted,geometric)>=0)transmitted=refract(ray.d,geometric,entering?1/m.params.y:m.params.y);
   float3 ambient=(Options.w&16)!=0?float3(0,0,0):lerp(float3(.024,.027,.033),float3(.075,.083,.10),saturate(n.y*.5+.5));
   result+=ray.weight*(directLighting(p,n,-ray.d,m,kind==1)+m.colorType.xyz*wd*(ambient+causticIrradiance(p,hit.meta.x)/PI));
   ray.rough=m.params.x>.12?1:0;OpticalRay secondary=ray;secondary.depth=ray.depth+1;float3 secondarySide,primarySide;
   if(wt>max(wr.x,max(wr.y,wr.z))){secondary.d=reflected;secondary.weight*=wr;secondarySide=geometric;ray.d=transmitted;ray.weight*=wt;ray.medium=entering?int(hit.meta.x):-1;primarySide=-geometric;}
   else{secondary.d=transmitted;secondary.weight*=wt;secondary.medium=entering?int(hit.meta.x):-1;secondarySide=-geometric;ray.d=reflected;ray.weight*=wr;primarySide=geometric;}
   secondary.o=p+secondarySide*.0005;
   if(size<8&&max(secondary.weight.x,max(secondary.weight.y,secondary.weight.z))>.012)stack[size++]=secondary;
   if(max(ray.weight.x,max(ray.weight.y,ray.weight.z))<.006)break;
   // Fixed cone quadrature approximates a rough lobe without temporal grain.
   // Keep secondary scattering bounded; the stochastic integrator samples it fully.
   if(m.params.x>.12&&ray.depth<3&&size<=5){
    float3 ideal=ray.d;ray.weight/=3;
    [unroll]for(int k=1;k<3;k++){OpticalRay branch=ray;branch.depth=ray.depth+1;branch.d=roughDirection(ideal,primarySide,m.params.x,float2(.5,k/3.0));branch.o=p+primarySide*.0005;stack[size++]=branch;}
    ray.d=roughDirection(ideal,primarySide,m.params.x,float2(.5,0));
   }
   ray.o=p+primarySide*.0005;
  }
 }return min(result,2000);
}
float3 path(float3 o,float3 d,inout uint seed,int channel){
 float3 radiance=0,throughput=1;int medium=-1;bool specular=true,hadDiffuse=false;
 [loop]for(uint bounce=0;bounce<Options.x;bounce++){
  uint primitive;float distance;if(!intersect(o,d,300,primitive,distance)){radiance+=throughput*environment(d);break;}
  Surface hit=Triangles[primitive];Material m=Materials[hit.meta.x];float3 p=o+d*distance;
  bool entering=dot(d,hit.n.xyz)<0;float3 geometric=entering?hit.n.xyz:-hit.n.xyz,n;surfaceDetail(m,p,geometric,n);if(medium>=0)throughput*=attenuation(Materials[medium],distance);
  int kind=int(m.colorType.w);
  if(kind==3){if(bounce==0){o=p+d*.001;continue;}uint lamp=max(0,int(m.params.w)-1);if(specular&&!hadDiffuse)radiance+=throughput*m.colorType.xyz*m.params.z*cone(lamp,-d);break;}
  float3 reflected,transmitted,wr;float wt,wd;surfaceLobes(m,d,n,entering,channel,reflected,transmitted,wr,wt,wd);
  if(dot(reflected,geometric)<=0)reflected=reflect(d,geometric);
  float3 illumination=directLighting(p,n,-d,m,kind==1)+m.colorType.xyz*wd/PI*causticIrradiance(p,hit.meta.x);if(hadDiffuse)illumination=min(illumination,4);radiance+=throughput*illumination;
  float pr=kind==1?1:wr.x,choice=rnd(seed);specular=m.params.x<=.12;
  if(choice<pr){throughput*=wr/max(.00001,pr);d=roughDirection(reflected,geometric,m.params.x,float2(rnd(seed),rnd(seed)));o=p+geometric*.0005;}
  else if(choice<pr+wt){d=roughDirection(transmitted,-geometric,m.params.x,float2(rnd(seed),rnd(seed)));medium=entering?int(hit.meta.x):-1;o=p-geometric*.0005;}
  else{throughput*=m.colorType.xyz;d=basisSample(n,float2(rnd(seed),rnd(seed)));if(dot(d,geometric)<0)d=basisSample(geometric,float2(rnd(seed),rnd(seed)));o=p+geometric*.0005;specular=false;hadDiffuse=true;}
  if(bounce>5){float survival=clamp(max(throughput.x,max(throughput.y,throughput.z)),.1,.98);if(rnd(seed)>survival)break;throughput/=survival;}
 }return min(radiance,2000);
}
[numthreads(8,8,1)]void PathTrace(uint3 id:SV_DispatchThreadID){
 if(id.x>=Counts.x||id.y>=Counts.y)return;uint seed=hash(id.x+id.y*Counts.x+Counts.z*18593+uint(RenderParams.w)*1979);float3 result=0;
 [loop]for(uint s=0;s<Counts.w;s++){
  bool stable=(Options.w&8)!=0;uint triplet=(Counts.z+s)/3;
  // RGB bands share a ray footprint. At the start of every moving frame use the
  // same pixel center; additional AA samples use a deterministic low-discrepancy set.
  float2 jitter=triplet==0?float2(.5,.5):frac(float2(.5,.5)+float2(.754877666,.569840296)*triplet);
  float3 direction=primaryRay(float2(id.xy)+(stable?jitter:float2(rnd(seed),rnd(seed))));float3 origin=CameraOrigin.xyz;
  if(Settings.z>0){float a=2*PI*rnd(seed),r=sqrt(rnd(seed))*Settings.z;float3 focus=origin+direction*(Settings.w/dot(direction,CameraForward.xyz));origin+=(CameraRight.xyz*cos(a)+CameraUp.xyz*sin(a))*r;direction=normalize(focus-origin);}
  int channel=Options.z!=0?int((Counts.z+s+(stable?0:hash(id.x+id.y*Counts.x)))%3):1;float3 c=stable?stablePath(origin,direction,channel):path(origin,direction,seed,channel);
  if(Options.z!=0)c*=channel==0?float3(3,0,0):channel==1?float3(0,3,0):float3(0,0,3);result+=c;
 }
 Accumulation[id.xy]=float4((Counts.z==0?0:Accumulation[id.xy].xyz)+result,1);
 float3 d=primaryRay(float2(id.xy)+.5),origin=CameraOrigin.xyz;float travelled=0;Guide[id.xy]=float4(0,0,0,300);
 // Guide denoising by the objects seen through the panes, rather than smoothing
 // the entire view as if the front window were one opaque flat surface.
 [loop]for(int step=0;step<24;step++){
  uint primitive;float distance;if(!intersect(origin,d,300-travelled,primitive,distance))break;Surface hit=Triangles[primitive];
  if(!glassWall(Materials[hit.meta.x])){float3 n=hit.n.xyz;if(dot(n,d)>0)n=-n;Guide[id.xy]=float4(n,travelled+distance);break;}
  travelled+=distance+.0005;if(travelled>=300)break;origin+=d*(distance+.0005);
 }
}
[numthreads(8,8,1)]void Temporal(uint3 id:SV_DispatchThreadID){
 if(id.x>=Counts.x||id.y>=Counts.y)return;float samples=max(1,float(Counts.z+Counts.w));float3 current=Accumulation[id.xy].xyz/samples;float historyCount=samples;
 if(Options.y!=0&&(Options.w&3)==3){
  float4 g=Guide[id.xy];float3 world=CameraOrigin.xyz+primaryRay(float2(id.xy)+.5)*g.w;float3 offset=world-PreviousOrigin.xyz;float z=dot(offset,PreviousForward.xyz);
  float2 uv=float2(dot(offset,PreviousRight.xyz)/(z*Settings.x*Settings.y),-dot(offset,PreviousUp.xyz)/(z*Settings.x))*.5+.5;int2 q=int2(uv*float2(Counts.xy));
  if(z>0&&all(q>=0)&&all(q<int2(Counts.xy))){
   float4 prevGuide=HistoryGuide[q];float expected=length(offset);float4 previous=HistoryColor[q];
   if(dot(prevGuide.xyz,g.xyz)>.96&&abs(prevGuide.w-expected)<.035+.002*expected){
    float3 mean=0,second=0;float n=0;
    [unroll]for(int y=-1;y<=1;y++)[unroll]for(int x=-1;x<=1;x++){int2 a=clamp(int2(id.xy)+int2(x,y),int2(0,0),int2(Counts.xy)-1);float3 c=Accumulation[a].xyz/samples;mean+=c;second+=c*c;n++;}
    mean/=n;float3 sigma=sqrt(max(0,second/n-mean*mean));float3 old=clamp(previous.xyz,mean-2.5*sigma-.03,mean+2.5*sigma+.03);float count=min(previous.w,96);float weight=count/(count+samples);
    current=lerp(current,old,min((Options.w&8)!=0?.25:.82,weight));historyCount=min(192,samples+count);
   }
  }
 }Filtered[id.xy]=float4(current,historyCount);
}
float edgeWeight(float4 a,float4 b){return pow(saturate(dot(a.xyz,b.xyz)),32)*exp(-abs(a.w-b.w)*12);}
[numthreads(8,8,1)]void Spatial(uint3 id:SV_DispatchThreadID){
 if(id.x>=Counts.x||id.y>=Counts.y)return;float4 input=Filtered[id.xy];float samples=max(1,float(Counts.z+Counts.w));
 if(Options.y==0||samples>2048||(Options.w&8)!=0){DenoiseTemp[id.xy]=input;return;}float4 g=Guide[id.xy];float3 sum=0;float weights=0;float lum=luminance(input.xyz);
 [unroll]for(int y=-2;y<=2;y++)[unroll]for(int x=-2;x<=2;x++){
  int2 q=clamp(int2(id.xy)+int2(x,y),int2(0,0),int2(Counts.xy)-1);float3 c=Filtered[q].xyz;float w=exp(-.22*(x*x+y*y))*edgeWeight(g,Guide[q]);w*=exp(-abs(luminance(c)-lum)/(1+lum)*.65);sum+=c*w;weights+=w;
 }float amount=(Options.w&1)!=0?1:saturate(1-samples/2048);DenoiseTemp[id.xy]=float4(lerp(input.xyz,sum/max(.001,weights),amount),input.w);
}
float3 aces(float3 x){return saturate((x*(2.51*x+.03))/(x*(2.43*x+.59)+.14));}
[numthreads(8,8,1)]void Resolve(uint3 id:SV_DispatchThreadID){
 if(id.x>=Counts.x||id.y>=Counts.y)return;float samples=max(1,float(Counts.z+Counts.w));float3 color=DenoiseTemp[id.xy].xyz;
 if(Options.y!=0&&(Options.w&8)==0&&((Options.w&1)!=0||samples<256)){
  float4 g=Guide[id.xy];float3 sum=0;float weights=0;
  [unroll]for(int y=-2;y<=2;y++)[unroll]for(int x=-2;x<=2;x++){int2 q=clamp(int2(id.xy)+int2(x,y)*2,int2(0,0),int2(Counts.xy)-1);float w=exp(-.35*(x*x+y*y))*edgeWeight(g,Guide[q]);sum+=DenoiseTemp[q].xyz*w;weights+=w;}
  color=lerp(color,sum/max(.001,weights),(Options.w&1)!=0?.8:.5);
 }
 float2 uv=float2(id.xy)/float2(Counts.xy)*2-1;float vignette=1-.06*dot(uv,uv);color=pow(max(0,aces(color*RenderParams.x)*vignette),1/2.2);Resolved[id.xy]=float4(color,1);
}
float3 resolvedPixel(int2 p){return Resolved[clamp(p,int2(0,0),int2(Counts.xy)-1)].rgb;}
float3 resolvedSample(float2 p){
 int2 q=int2(floor(p));float2 f=frac(p);
 return lerp(lerp(resolvedPixel(q),resolvedPixel(q+int2(1,0)),f.x),lerp(resolvedPixel(q+int2(0,1)),resolvedPixel(q+int2(1,1)),f.x),f.y);
}
// Spatial edge antialiasing after tone mapping. No random offsets or history:
// it works on the first moving frame and never filters the ImGui overlay.
[numthreads(8,8,1)]void Antialias(uint3 id:SV_DispatchThreadID){
 if(id.x>=Counts.x||id.y>=Counts.y)return;int2 p=int2(id.xy);float3 center=resolvedPixel(p);
 if((Options.w&32)==0){Display[p]=float4(center,1);return;}
 float nw=luminance(resolvedPixel(p+int2(-1,-1))),ne=luminance(resolvedPixel(p+int2(1,-1)));
 float sw=luminance(resolvedPixel(p+int2(-1,1))),se=luminance(resolvedPixel(p+int2(1,1))),m=luminance(center);
 float lo=min(m,min(min(nw,ne),min(sw,se))),hi=max(m,max(max(nw,ne),max(sw,se)));
 if(hi-lo<max(.025,hi*.10)){Display[p]=float4(center,1);return;}
 float2 direction=float2(-(nw+ne-sw-se),nw+sw-ne-se);
 float reduce=max((nw+ne+sw+se)*(.25*.125),1.0/128);
 direction=clamp(direction/(min(abs(direction.x),abs(direction.y))+reduce),-8,8);
 float3 a=.5*(resolvedSample(float2(p)-direction/6)+resolvedSample(float2(p)+direction/6));
 float3 b=a*.5+.25*(resolvedSample(float2(p)-direction*.5)+resolvedSample(float2(p)+direction*.5));
 float l=luminance(b);Display[p]=float4(l<lo||l>hi?a:b,1);
}
