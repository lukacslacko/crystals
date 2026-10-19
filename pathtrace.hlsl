struct Surface { float4 a,b,c,n; uint4 meta; };
struct Material { float4 colorType,params,absorption; };
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
float cone(uint i,float3 direction){return smoothstep(LightDirection[i].w,LightColor[i].w,dot(direction,LightDirection[i].xyz));}
float3 primaryRay(float2 pixel){float2 uv=pixel/float2(Counts.xy)*2-1;return normalize(CameraForward.xyz+CameraRight.xyz*(uv.x*Settings.x*Settings.y)-CameraUp.xyz*(uv.y*Settings.x));}
bool intersect(float3 o,float3 d,float maximum,out uint primitive,out float distance){
 RayDesc ray;ray.Origin=o;ray.Direction=d;ray.TMin=.00015;ray.TMax=maximum;
 RayQuery<RAY_FLAG_FORCE_OPAQUE> query;query.TraceRayInline(Scene,RAY_FLAG_NONE,255,ray);while(query.Proceed()){}
 if(query.CommittedStatus()==COMMITTED_TRIANGLE_HIT){primitive=query.CommittedPrimitiveIndex();distance=query.CommittedRayT();return true;}
 primitive=0;distance=maximum;return false;
}
float visibility(float3 o,float3 d,float maximum){uint primitive;float t;return intersect(o,d,maximum,primitive,t)?0:1;}
bool glassScatter(inout float3 d,float3 n,bool entering,float ior,inout uint seed){
 float eta=entering?1/ior:ior,c=min(1,-dot(d,n));float r0=(ior-1)/(ior+1);r0*=r0;
 float f=r0+(1-r0)*pow(1-c,5);float3 refracted=refract(d,n,eta);
 bool transmission=dot(refracted,refracted)>.01&&rnd(seed)>=f;
 d=transmission?normalize(refracted):reflect(d,n);return transmission;
}
float3 environment(float3 d){return lerp(float3(.004,.006,.011),float3(.025,.033,.05),smoothstep(-.15,.8,d.y));}
bool receiverUV(float3 p,uint material,out float2 pixel,out float area){
 if(material==3){pixel=float2((p.x+12)/24*2048,(p.y+.58)/22*1024);area=528.0/(2048*1024);return all(pixel>=0)&&pixel.x<2047&&pixel.y<1023;}
 if(material==0){pixel=float2((p.x+12)/24*2048,(p.z+8)/20*1024+1024);area=480.0/(2048*1024);return pixel.x>=0&&pixel.x<2047&&pixel.y>=1024&&pixel.y<2047;}
 pixel=0;area=1;return false;
}
float3 photonAt(int2 q){return float3(PhotonMap[int3(q,0)],PhotonMap[int3(q,1)],PhotonMap[int3(q,2)]);}
float3 causticIrradiance(float3 p,uint material){
 if((Options.w&4)==0)return 0;float2 pixel;float area;if(!receiverUV(p,material,pixel,area))return 0;
 int2 q=int2(pixel);float2 f=frac(pixel);
 float3 a=lerp(PhotonFiltered[q].xyz,PhotonFiltered[q+int2(1,0)].xyz,f.x);
 float3 b=lerp(PhotonFiltered[q+int2(0,1)].xyz,PhotonFiltered[q+int2(1,1)].xyz,f.x);
 return lerp(a,b,f.y)/PHOTON_SCALE*RenderParams.z;
}
void deposit(float3 p,uint material,float3 flux){
 float2 pixel;float area;if(!receiverUV(p,material,pixel,area))return;
 int2 base=int2(pixel);float3 energy=max(0,flux/area*PHOTON_SCALE);
 [unroll]for(int y=-2;y<=2;y++)[unroll]for(int x=-2;x<=2;x++){
  int2 q=base+int2(x,y);if(q.x<0||q.x>=2048||q.y<0||q.y>=2048)continue;
  float w=exp(-.5*(x*x+y*y))/6.168924;uint3 e=uint3(min(energy*w,10000000));
  if(e.x)InterlockedAdd(PhotonMap[int3(q,0)],e.x);if(e.y)InterlockedAdd(PhotonMap[int3(q,1)],e.y);if(e.z)InterlockedAdd(PhotonMap[int3(q,2)],e.z);
 }
}
[numthreads(8,8,1)]void ClearPhotons(uint3 id:SV_DispatchThreadID){if(all(id.xy<2048)){PhotonMap[uint3(id.xy,0)]=0;PhotonMap[uint3(id.xy,1)]=0;PhotonMap[uint3(id.xy,2)]=0;}}
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
  if(medium>=0)flux*=exp(-Materials[medium].absorption.xyz*distance);if(max(flux.x,max(flux.y,flux.z))<1e-10)return;
  bool entering=dot(d,hit.n.xyz)<0;float3 n=entering?hit.n.xyz:-hit.n.xyz;int kind=int(m.colorType.w);
  if(kind==2){float ior=m.params.y+(channel==0?-1:channel==2?1:0)*m.absorption.w;if(glassScatter(d,n,entering,ior,seed))medium=entering?int(hit.meta.x):-1;touchedCrystal=true;}
  else if(kind==1){d=reflect(d,n);flux*=m.colorType.xyz;}
  else{if(kind==0&&touchedCrystal)deposit(p,hit.meta.x,flux);return;}
  origin=p+d*.0005;
 }
}
[numthreads(8,8,1)]void PhotonHorizontal(uint3 id:SV_DispatchThreadID){
 if(any(id.xy>=2048))return;float3 sum=0;float weights=0;
 float variance=RenderParams.y<2000000?24:4.5;int radius=RenderParams.y<2000000?10:4;
 [loop]for(int x=-radius;x<=radius;x++){float w=exp(-float(x*x)/variance);sum+=photonAt(int2(clamp(int(id.x)+x,0,2047),id.y))*w;weights+=w;}
 PhotonTemp[id.xy]=float4(sum/weights,1);
}
[numthreads(8,8,1)]void PhotonVertical(uint3 id:SV_DispatchThreadID){
 if(any(id.xy>=2048))return;float3 sum=0;float weights=0;int low=id.y<1024?0:1024;
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
  float beam=cone(i,-l);if(nl<=0||beam<=0)continue;float3 brdf=metallic?metalBRDF(n,view,l,m.colorType.xyz,m.params.x):m.colorType.xyz/PI;
  sum+=brdf*LightColor[i].xyz*(beam*nl/r2)*visibility(p+n*.0005,l,distance-.002);
 }return sum;
}
struct OpticalRay {float3 o,d,weight;int medium;uint depth;};
// Bounded Whitted integration: no Russian roulette or random Fresnel decisions.
// Small branches are deliberately truncated; diffuse indirect is an irradiance
// approximation. The stochastic integrator remains separately available.
float3 stablePath(float3 o,float3 d,int channel){
 OpticalRay stack[8];int size=1;stack[0].o=o;stack[0].d=d;stack[0].weight=1;stack[0].medium=-1;stack[0].depth=0;float3 result=0;
 [loop]for(uint work=0;work<72&&size>0;){
  OpticalRay ray=stack[--size];
  [loop]for(;ray.depth<Options.x&&work<72;ray.depth++){
   work++;uint primitive;float distance;
   if(!intersect(ray.o,ray.d,300,primitive,distance)){result+=ray.weight*environment(ray.d);break;}
   Surface hit=Triangles[primitive];Material m=Materials[hit.meta.x];float3 p=ray.o+ray.d*distance;
   bool entering=dot(ray.d,hit.n.xyz)<0;float3 n=entering?hit.n.xyz:-hit.n.xyz;
   if(ray.medium>=0)ray.weight*=exp(-Materials[ray.medium].absorption.xyz*distance);
   if(max(ray.weight.x,max(ray.weight.y,ray.weight.z))<.006)break;
   int kind=int(m.colorType.w);
   if(kind==2){
    float ior=m.params.y+(channel==0?-1:channel==2?1:0)*m.absorption.w;
    float cosine=saturate(-dot(ray.d,n)),eta=entering?1/ior:ior;float3 transmitted=refract(ray.d,n,eta),reflected=reflect(ray.d,n);
    float f0=(ior-1)/(ior+1);f0*=f0;float fresnel=f0+(1-f0)*pow(1-cosine,5);
    if(dot(transmitted,transmitted)<.01){ray.d=reflected;ray.o=p+ray.d*.0004;continue;}
    OpticalRay secondary=ray;secondary.depth=ray.depth+1;
    if(fresnel<.5){secondary.d=reflected;secondary.weight*=fresnel;ray.d=normalize(transmitted);ray.weight*=1-fresnel;ray.medium=entering?int(hit.meta.x):-1;}
    else {secondary.d=normalize(transmitted);secondary.weight*=1-fresnel;secondary.medium=entering?int(hit.meta.x):-1;ray.d=reflected;ray.weight*=fresnel;}
    secondary.o=p+secondary.d*.0004;
    if(size<8&&max(secondary.weight.x,max(secondary.weight.y,secondary.weight.z))>.012)stack[size++]=secondary;
    ray.o=p+ray.d*.0004;
   }else if(kind==1){
    result+=ray.weight*directLighting(p,n,-ray.d,m,true);float cosine=saturate(-dot(ray.d,n));ray.weight*=m.colorType.xyz+(1-m.colorType.xyz)*pow(1-cosine,5);ray.d=reflect(ray.d,n);ray.o=p+n*.0004;
   }else if(kind==3){
    if(ray.depth==0){ray.o=p+ray.d*.001;continue;}uint lamp=max(0,int(m.params.w)-1);result+=ray.weight*m.colorType.xyz*m.params.z*cone(lamp,-ray.d);break;
   }else{
    float3 ambient=lerp(float3(.024,.027,.033),float3(.075,.083,.10),saturate(n.y*.5+.5));
    float3 illumination=directLighting(p,n,-ray.d,m,false)+m.colorType.xyz*(causticIrradiance(p,hit.meta.x)/PI+ambient);
    result+=ray.weight*illumination;break;
   }
  }
 }return min(result,2000);
}
float3 path(float3 o,float3 d,inout uint seed,int channel){
 float3 radiance=0,throughput=1;int medium=-1;bool specular=true,hadDiffuse=false;
 [loop]for(uint bounce=0;bounce<Options.x;bounce++){
  uint primitive;float distance;if(!intersect(o,d,300,primitive,distance)){radiance+=throughput*environment(d);break;}
  Surface hit=Triangles[primitive];Material m=Materials[hit.meta.x];float3 p=o+d*distance;
  bool entering=dot(d,hit.n.xyz)<0;float3 n=entering?hit.n.xyz:-hit.n.xyz;if(medium>=0)throughput*=exp(-Materials[medium].absorption.xyz*distance);
  int kind=int(m.colorType.w);
  if(kind==3){if(bounce==0){o=p+d*.001;continue;}uint lamp=max(0,int(m.params.w)-1);if(specular&&!hadDiffuse)radiance+=throughput*m.colorType.xyz*m.params.z*cone(lamp,-d);break;}
  if(kind==2){float ior=m.params.y+(channel==0?-1:channel==2?1:0)*m.absorption.w;if(glassScatter(d,n,entering,ior,seed))medium=entering?int(hit.meta.x):-1;o=p+d*.0004;specular=true;}
  else if(kind==1){
   float grain=sin(p.y*1900+p.x*13)*.003;n=normalize(n+float3(grain,grain*.14,0));float3 metalLight=directLighting(p,n,-d,m,true);if(hadDiffuse)metalLight=min(metalLight,2.5);radiance+=throughput*metalLight;
   float3 reflected=reflect(d,n);float cosine=max(0,-dot(n,d));d=normalize(lerp(reflected,basisSample(n,float2(rnd(seed),rnd(seed))),m.params.x*m.params.x));if(dot(d,n)<0)d=reflected;
   throughput*=m.colorType.xyz+(1-m.colorType.xyz)*pow(1-cosine,5);o=p+n*.0004;specular=false;
  }else{
   if(hit.meta.x==0)m.colorType.xyz*=.985+.025*sin(p.x*23+sin(p.z*17))*sin(p.z*31);
   float3 illumination=directLighting(p,n,-d,m,false)+m.colorType.xyz/PI*causticIrradiance(p,hit.meta.x);if(hadDiffuse)illumination=min(illumination,4);radiance+=throughput*illumination;
   throughput*=m.colorType.xyz;d=basisSample(n,float2(rnd(seed),rnd(seed)));o=p+n*.0004;specular=false;hadDiffuse=true;
  }
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
 float3 d=primaryRay(float2(id.xy)+.5);uint primitive;float distance;
 if(intersect(CameraOrigin.xyz,d,300,primitive,distance)){float3 n=Triangles[primitive].n.xyz;if(dot(n,d)>0)n=-n;Guide[id.xy]=float4(n,distance);}else Guide[id.xy]=float4(0,0,0,300);
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
 float2 uv=float2(id.xy)/float2(Counts.xy)*2-1;float vignette=1-.06*dot(uv,uv);color=pow(max(0,aces(color*RenderParams.x)*vignette),1/2.2);Display[id.xy]=float4(color,1);
}
