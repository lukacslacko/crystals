#include "renderer.h"
#include "imgui.h"
#include "imgui_impl_dx12.h"
#include "imgui_impl_win32.h"
#include <d3dcompiler.h>
#include <wincodec.h>
#include <fstream>
#include <stdexcept>
#include <cstring>

static void check(HRESULT hr,const char*what){if(FAILED(hr)){char b[256];sprintf_s(b,"%s (HRESULT 0x%08X)",what,unsigned(hr));throw std::runtime_error(b);}}
static void barrier(ID3D12GraphicsCommandList*list,ID3D12Resource*resource,D3D12_RESOURCE_STATES before,D3D12_RESOURCE_STATES after){D3D12_RESOURCE_BARRIER b{};b.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;b.Transition={resource,D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,before,after};list->ResourceBarrier(1,&b);}
static void uavBarrier(ID3D12GraphicsCommandList*list,ID3D12Resource*r){D3D12_RESOURCE_BARRIER b{};b.Type=D3D12_RESOURCE_BARRIER_TYPE_UAV;b.UAV.pResource=r;list->ResourceBarrier(1,&b);}
static std::vector<char>binary(const std::wstring&p){std::ifstream f(p,std::ios::binary|std::ios::ate);if(!f)throw std::runtime_error("A shader file is missing beside CrystalSlab.exe");auto n=f.tellg();std::vector<char>b(size_t(n),0);f.seekg(0);f.read(b.data(),n);return b;}
ComPtr<ID3D12Resource> Renderer::buffer(UINT64 bytes,D3D12_HEAP_TYPE type,D3D12_RESOURCE_FLAGS flags,D3D12_RESOURCE_STATES state){
 D3D12_HEAP_PROPERTIES hp{};hp.Type=type;D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;d.Width=std::max<UINT64>(256,bytes);d.Height=1;d.DepthOrArraySize=1;d.MipLevels=1;d.SampleDesc.Count=1;d.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;d.Flags=flags;ComPtr<ID3D12Resource>r;check(device->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&d,state,nullptr,IID_PPV_ARGS(&r)),"Create GPU buffer");return r;
}
void Renderer::init(HWND hwnd,uint32_t w,uint32_t h,const std::wstring&directory,bool ui){
 window=hwnd;width=w;height=h;uiEnabled=ui;
 ComPtr<IDXGIFactory6>factory;check(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory)),"Create DXGI factory");
 ComPtr<IDXGIAdapter1>adapter;
 for(UINT index=0;factory->EnumAdapterByGpuPreference(index,DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,IID_PPV_ARGS(&adapter))!=DXGI_ERROR_NOT_FOUND;index++){
  DXGI_ADAPTER_DESC1 desc{};adapter->GetDesc1(&desc);if(desc.Flags&DXGI_ADAPTER_FLAG_SOFTWARE){adapter.Reset();continue;}
  if(SUCCEEDED(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_12_1,IID_PPV_ARGS(&device)))){
   D3D12_FEATURE_DATA_D3D12_OPTIONS5 options{};device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5,&options,sizeof(options));if(options.RaytracingTier>=D3D12_RAYTRACING_TIER_1_1){char name[256];WideCharToMultiByte(CP_UTF8,0,desc.Description,-1,name,256,nullptr,nullptr);adapterName=name;break;}device.Reset();
  }adapter.Reset();
 }
 if(!device)throw std::runtime_error("DirectX Raytracing tier 1.1 is required. Select the NVIDIA RTX graphics adapter.");
 D3D12_COMMAND_QUEUE_DESC q{};q.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;check(device->CreateCommandQueue(&q,IID_PPV_ARGS(&queue)),"Create command queue");
 check(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator)),"Create command allocator");
 check(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&list)),"Create command list");list->Close();
 check(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)),"Create fence");eventHandle=CreateEvent(nullptr,FALSE,FALSE,nullptr);
 DXGI_SWAP_CHAIN_DESC1 sd{};sd.Width=width;sd.Height=height;sd.Format=DXGI_FORMAT_R8G8B8A8_UNORM;sd.SampleDesc.Count=1;sd.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;sd.BufferCount=2;sd.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;sd.Scaling=DXGI_SCALING_STRETCH;
 ComPtr<IDXGISwapChain1>s1;check(factory->CreateSwapChainForHwnd(queue.Get(),hwnd,&sd,nullptr,nullptr,&s1),"Create native swapchain");s1.As(&swap);factory->MakeWindowAssociation(hwnd,DXGI_MWA_NO_ALT_ENTER);
 D3D12_DESCRIPTOR_HEAP_DESC hd{};hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;hd.NumDescriptors=16;hd.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;check(device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&heap)),"Create descriptor heap");
 hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV;hd.NumDescriptors=2;hd.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_NONE;check(device->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&rtvHeap)),"Create RTV heap");rtvSize=device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
 D3D12_DESCRIPTOR_RANGE range{};range.RangeType=D3D12_DESCRIPTOR_RANGE_TYPE_UAV;range.NumDescriptors=10;range.BaseShaderRegister=0;
 D3D12_ROOT_PARAMETER params[5]{};params[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;params[0].DescriptorTable={1,&range};
 for(int i=1;i<4;i++){params[i].ParameterType=D3D12_ROOT_PARAMETER_TYPE_SRV;params[i].Descriptor.ShaderRegister=i-1;}
 params[4].ParameterType=D3D12_ROOT_PARAMETER_TYPE_CBV;params[4].Descriptor.ShaderRegister=0;
 D3D12_ROOT_SIGNATURE_DESC rd{};rd.NumParameters=5;rd.pParameters=params;ComPtr<ID3DBlob>blob,errors;check(D3D12SerializeRootSignature(&rd,D3D_ROOT_SIGNATURE_VERSION_1,&blob,&errors),"Serialize root signature");check(device->CreateRootSignature(0,blob->GetBufferPointer(),blob->GetBufferSize(),IID_PPV_ARGS(&root)),"Create root signature");
 auto makePSO=[&](const wchar_t*file,ComPtr<ID3D12PipelineState>&pso){auto b=binary(directory+file);D3D12_COMPUTE_PIPELINE_STATE_DESC d{};d.pRootSignature=root.Get();d.CS={b.data(),b.size()};check(device->CreateComputePipelineState(&d,IID_PPV_ARGS(&pso)),"Create ray-query compute pipeline");};
 makePSO(L"PathTrace.cso",pathPSO);makePSO(L"Resolve.cso",resolvePSO);makePSO(L"ClearPhotons.cso",clearPhotonsPSO);makePSO(L"PhotonTrace.cso",photonPSO);makePSO(L"Temporal.cso",temporalPSO);makePSO(L"Spatial.cso",spatialPSO);makePSO(L"PhotonHorizontal.cso",photonHPSO);makePSO(L"PhotonVertical.cso",photonVPSO);
 constantBuffer=buffer(sizeof(GPUConstants),D3D12_HEAP_TYPE_UPLOAD);textures();
 D3D12_QUERY_HEAP_DESC query{};query.Type=D3D12_QUERY_HEAP_TYPE_TIMESTAMP;query.Count=2;check(device->CreateQueryHeap(&query,IID_PPV_ARGS(&timerHeap)),"Create GPU timestamp queries");timerReadback=buffer(16,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_FLAG_NONE,D3D12_RESOURCE_STATE_COPY_DEST);queue->GetTimestampFrequency(&timestampFrequency);
 if(ui){IMGUI_CHECKVERSION();ImGui::CreateContext();ImGui::GetIO().IniFilename=nullptr;ImGui_ImplWin32_Init(hwnd);auto cpu=heap->GetCPUDescriptorHandleForHeapStart();auto gpu=heap->GetGPUDescriptorHandleForHeapStart();auto size=device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);cpu.ptr+=12*size;gpu.ptr+=12*size;check(ImGui_ImplDX12_Init(device.Get(),2,DXGI_FORMAT_R8G8B8A8_UNORM,heap.Get(),cpu,gpu)?S_OK:E_FAIL,"Initialize native UI");}
}
void Renderer::wait(){if(!queue)return;UINT64 value=++fenceValue;check(queue->Signal(fence.Get(),value),"Signal GPU fence");if(fence->GetCompletedValue()<value){check(fence->SetEventOnCompletion(value,eventHandle),"Wait for GPU");WaitForSingleObject(eventHandle,INFINITE);}}
void Renderer::textures(){
 for(int i=0;i<2;i++){check(swap->GetBuffer(i,IID_PPV_ARGS(&backbuffers[i])),"Get swapchain buffer");auto handle=rtvHeap->GetCPUDescriptorHandleForHeapStart();handle.ptr+=i*rtvSize;device->CreateRenderTargetView(backbuffers[i].Get(),nullptr,handle);}
 auto make=[&](DXGI_FORMAT format,ComPtr<ID3D12Resource>&r,UINT w=0,UINT h=0,UINT layers=1){D3D12_HEAP_PROPERTIES hp{};hp.Type=D3D12_HEAP_TYPE_DEFAULT;D3D12_RESOURCE_DESC d{};d.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;d.Width=w?w:width;d.Height=h?h:height;d.DepthOrArraySize=UINT16(layers);d.MipLevels=1;d.Format=format;d.SampleDesc.Count=1;d.Flags=D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;check(device->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&d,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,nullptr,IID_PPV_ARGS(&r)),"Create render texture");};
 auto fp=DXGI_FORMAT_R32G32B32A32_FLOAT;make(fp,accum);make(DXGI_FORMAT_R8G8B8A8_UNORM,display);make(fp,guide);make(DXGI_FORMAT_R32_UINT,photonMap,4096,2048,3);make(fp,historyColor);make(fp,historyGuide);make(fp,filtered);make(fp,denoiseTemp);make(fp,photonTemp,4096,2048);make(fp,photonFiltered,4096,2048);
 ID3D12Resource*rs[]={accum.Get(),display.Get(),guide.Get(),photonMap.Get(),historyColor.Get(),historyGuide.Get(),filtered.Get(),denoiseTemp.Get(),photonTemp.Get(),photonFiltered.Get()};auto h=heap->GetCPUDescriptorHandleForHeapStart();auto stride=device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);for(int i=0;i<10;i++){D3D12_UNORDERED_ACCESS_VIEW_DESC d{};d.Format=i==1?DXGI_FORMAT_R8G8B8A8_UNORM:i==3?DXGI_FORMAT_R32_UINT:fp;d.ViewDimension=i==3?D3D12_UAV_DIMENSION_TEXTURE2DARRAY:D3D12_UAV_DIMENSION_TEXTURE2D;if(i==3)d.Texture2DArray.ArraySize=3;device->CreateUnorderedAccessView(rs[i],nullptr,&d,h);h.ptr+=stride;}samples=0;historyValid=false;photonsDirty=true;
}
void Renderer::resize(uint32_t w,uint32_t h){if(!w||!h||w==width&&h==height)return;wait();for(auto&r:backbuffers)r.Reset();accum.Reset();display.Reset();guide.Reset();width=w;height=h;check(swap->ResizeBuffers(2,w,h,DXGI_FORMAT_R8G8B8A8_UNORM,0),"Resize native swapchain");textures();}
void Renderer::setScene(const CrystalScene&s){
 wait();triangleCount=s.triangles.size();if(!triBuffer||triBuffer->GetDesc().Width<triangleCount*sizeof(Triangle))triBuffer=buffer(triangleCount*sizeof(Triangle),D3D12_HEAP_TYPE_UPLOAD);if(!vertexBuffer||vertexBuffer->GetDesc().Width<triangleCount*3*sizeof(V3))vertexBuffer=buffer(triangleCount*3*sizeof(V3),D3D12_HEAP_TYPE_UPLOAD);if(!materialBuffer||materialBuffer->GetDesc().Width<s.materials.size()*sizeof(Material))materialBuffer=buffer(s.materials.size()*sizeof(Material),D3D12_HEAP_TYPE_UPLOAD);
 void*p;check(triBuffer->Map(0,nullptr,&p),"Map geometry");memcpy(p,s.triangles.data(),triangleCount*sizeof(Triangle));triBuffer->Unmap(0,nullptr);
 check(vertexBuffer->Map(0,nullptr,&p),"Map vertices");auto v=static_cast<V3*>(p);for(auto&t:s.triangles){*v++=t.a.xyz();*v++=t.b.xyz();*v++=t.c.xyz();}vertexBuffer->Unmap(0,nullptr);
 check(materialBuffer->Map(0,nullptr,&p),"Map materials");memcpy(p,s.materials.data(),s.materials.size()*sizeof(Material));materialBuffer->Unmap(0,nullptr);dirty=true;photonsDirty=true;samples=0;historyValid=false;lastSceneChange=std::chrono::steady_clock::now();
}
void Renderer::buildAS(){
 D3D12_RAYTRACING_GEOMETRY_DESC geometry{};geometry.Type=D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;geometry.Flags=D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;geometry.Triangles.VertexBuffer={vertexBuffer->GetGPUVirtualAddress(),sizeof(V3)};geometry.Triangles.VertexCount=UINT(triangleCount*3);geometry.Triangles.VertexFormat=DXGI_FORMAT_R32G32B32_FLOAT;
 D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS input{};input.Type=D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;input.DescsLayout=D3D12_ELEMENTS_LAYOUT_ARRAY;input.NumDescs=1;input.pGeometryDescs=&geometry;input.Flags=D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
 D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info{};device->GetRaytracingAccelerationStructurePrebuildInfo(&input,&info);
 blas=buffer(info.ResultDataMaxSizeInBytes,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);
 D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS top{};top.Type=D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;top.DescsLayout=D3D12_ELEMENTS_LAYOUT_ARRAY;top.NumDescs=1;top.Flags=D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO topInfo{};device->GetRaytracingAccelerationStructurePrebuildInfo(&top,&topInfo);
 tlas=buffer(topInfo.ResultDataMaxSizeInBytes,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);
 scratch=buffer(std::max(info.ScratchDataSizeInBytes,topInfo.ScratchDataSizeInBytes),D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
 D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC build{};build.Inputs=input;build.DestAccelerationStructureData=blas->GetGPUVirtualAddress();build.ScratchAccelerationStructureData=scratch->GetGPUVirtualAddress();list->BuildRaytracingAccelerationStructure(&build,0,nullptr);uavBarrier(list.Get(),blas.Get());uavBarrier(list.Get(),scratch.Get());
 instances=buffer(sizeof(D3D12_RAYTRACING_INSTANCE_DESC),D3D12_HEAP_TYPE_UPLOAD);D3D12_RAYTRACING_INSTANCE_DESC instance{};instance.Transform[0][0]=instance.Transform[1][1]=instance.Transform[2][2]=1;instance.InstanceMask=255;instance.AccelerationStructure=blas->GetGPUVirtualAddress();void*p;instances->Map(0,nullptr,&p);memcpy(p,&instance,sizeof(instance));instances->Unmap(0,nullptr);
 top.InstanceDescs=instances->GetGPUVirtualAddress();build.Inputs=top;build.DestAccelerationStructureData=tlas->GetGPUVirtualAddress();list->BuildRaytracingAccelerationStructure(&build,0,nullptr);uavBarrier(list.Get(),tlas.Get());dirty=false;
}
void Renderer::render(const CrystalScene&s,const Camera&camera,bool drawUI){
 wait();
 if(timerValid){UINT64*times=nullptr;D3D12_RANGE range{0,16};timerReadback->Map(0,&range,reinterpret_cast<void**>(&times));gpuMs=float(double(times[1]-times[0])*1000/double(timestampFrequency));D3D12_RANGE written{0,0};timerReadback->Unmap(0,&written);
  if(automaticBudget&&previousTracing&&gpuMs>.05f){float ratio=std::clamp(gpuBudgetMs/gpuMs,.5f,2.f);uint32_t next=uint32_t(std::floor((spp*ratio+1.f)/3))*3;spp=std::clamp(next,3u,192u);}
 }
 auto now=std::chrono::steady_clock::now();bool sceneChanged=dirty;
 bool cameraChanged=cameraValid&&(dot(camera.origin()-previousCamera.origin(),camera.origin()-previousCamera.origin())>1e-10f||dot(camera.forward()-previousCamera.forward(),camera.forward()-previousCamera.forward())>1e-10f);
 if(cameraChanged){samples=0;lastMotion=now;}if(sceneChanged)lastMotion=now;
 moving=cameraChanged||sceneChanged||std::chrono::duration<float>(now-lastMotion).count()<.14f;
 bool dynamicGeometry=s.moving()||previewEdit;
 if(lowQualityPhotons&&!dynamicGeometry&&std::chrono::duration<float>(now-lastSceneChange).count()>.2f){photonsDirty=true;lowQualityPhotons=false;samples=0;historyValid=false;}
 bool bakePhotons=caustics&&photonsDirty;uint32_t photonPaths=dynamicGeometry?1048576:photons;
 check(allocator->Reset(),"Reset allocator");check(list->Reset(allocator.Get(),nullptr),"Reset command list");if(dirty)buildAS();
 bool tracing=samples<maxSamples;GPUConstants c{};c.origin=F4(camera.origin());c.forward=F4(camera.forward());c.right=F4(camera.right());c.up=F4(camera.up());c.settings={.345f,float(width)/height,moving?0:camera.aperture,camera.distance};
 c.counts={width,height,samples,tracing?spp:0};c.options={maxBounces,denoise?1u:0u,dispersion?1u:0u,(moving?1u:0u)|(historyValid?2u:0u)|(caustics?4u:0u)|(stableOptics?8u:0u)};
 for(int i=0;i<int(s.lights.size());i++){c.lightPosition[i]=F4(s.lights[i].position,s.lights[i].radius);c.lightDirection[i]=F4(s.lights[i].direction,s.lights[i].outerCos);c.lightColor[i]=F4(s.lights[i].intensity,s.lights[i].innerCos);}
 c.previousOrigin=F4(previousCamera.origin());c.previousForward=F4(previousCamera.forward());c.previousRight=F4(previousCamera.right());c.previousUp=F4(previousCamera.up());c.renderParams={1.05f,float(photonPaths),1,float(frameIndex)};bool panels=std::any_of(s.appearance.walls.begin(),s.appearance.walls.end(),[](int w){return w!=0;});c.sceneParams={float(s.lights.size()),s.depth,panels?1.f:0.f,s.boxHalf};
 void*p;constantBuffer->Map(0,nullptr,&p);memcpy(p,&c,sizeof(c));constantBuffer->Unmap(0,nullptr);
 ID3D12DescriptorHeap*heaps[]={heap.Get()};list->SetDescriptorHeaps(1,heaps);list->SetComputeRootSignature(root.Get());list->SetComputeRootDescriptorTable(0,heap->GetGPUDescriptorHandleForHeapStart());list->SetComputeRootShaderResourceView(1,tlas->GetGPUVirtualAddress());list->SetComputeRootShaderResourceView(2,triBuffer->GetGPUVirtualAddress());list->SetComputeRootShaderResourceView(3,materialBuffer->GetGPUVirtualAddress());list->SetComputeRootConstantBufferView(4,constantBuffer->GetGPUVirtualAddress());
 if(bakePhotons){list->SetPipelineState(clearPhotonsPSO.Get());list->Dispatch(panels?512:256,256,1);uavBarrier(list.Get(),photonMap.Get());list->SetPipelineState(photonPSO.Get());list->Dispatch((std::min(photonPaths,8388608u)+255)/256,(photonPaths+8388607)/8388608,1);uavBarrier(list.Get(),photonMap.Get());list->SetPipelineState(photonHPSO.Get());list->Dispatch(panels?512:256,256,1);uavBarrier(list.Get(),photonTemp.Get());list->SetPipelineState(photonVPSO.Get());list->Dispatch(panels?512:256,256,1);uavBarrier(list.Get(),photonFiltered.Get());photonsDirty=false;lowQualityPhotons=dynamicGeometry;}
 list->EndQuery(timerHeap.Get(),D3D12_QUERY_TYPE_TIMESTAMP,0);
 if(tracing){
  list->SetPipelineState(pathPSO.Get());list->Dispatch((width+7)/8,(height+7)/8,1);uavBarrier(list.Get(),accum.Get());uavBarrier(list.Get(),guide.Get());
  list->SetPipelineState(temporalPSO.Get());list->Dispatch((width+7)/8,(height+7)/8,1);uavBarrier(list.Get(),filtered.Get());
  list->SetPipelineState(spatialPSO.Get());list->Dispatch((width+7)/8,(height+7)/8,1);uavBarrier(list.Get(),denoiseTemp.Get());
 }
 list->SetPipelineState(resolvePSO.Get());list->Dispatch((width+7)/8,(height+7)/8,1);
 list->EndQuery(timerHeap.Get(),D3D12_QUERY_TYPE_TIMESTAMP,1);list->ResolveQueryData(timerHeap.Get(),D3D12_QUERY_TYPE_TIMESTAMP,0,2,timerReadback.Get(),0);
 if(tracing){
  auto copy=[&](ID3D12Resource*source,ID3D12Resource*dest){barrier(list.Get(),source,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COPY_SOURCE);barrier(list.Get(),dest,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COPY_DEST);list->CopyResource(dest,source);barrier(list.Get(),source,D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);barrier(list.Get(),dest,D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);};
  copy(filtered.Get(),historyColor.Get());copy(guide.Get(),historyGuide.Get());
 }
 UINT frame=swap->GetCurrentBackBufferIndex();barrier(list.Get(),display.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COPY_SOURCE);barrier(list.Get(),backbuffers[frame].Get(),D3D12_RESOURCE_STATE_PRESENT,D3D12_RESOURCE_STATE_COPY_DEST);list->CopyResource(backbuffers[frame].Get(),display.Get());barrier(list.Get(),display.Get(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
 if(drawUI&&uiEnabled){barrier(list.Get(),backbuffers[frame].Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_RENDER_TARGET);auto rtv=rtvHeap->GetCPUDescriptorHandleForHeapStart();rtv.ptr+=frame*rtvSize;list->OMSetRenderTargets(1,&rtv,FALSE,nullptr);ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(),list.Get());barrier(list.Get(),backbuffers[frame].Get(),D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_PRESENT);}else barrier(list.Get(),backbuffers[frame].Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_PRESENT);
 check(list->Close(),"Close render commands");ID3D12CommandList*commands[]={list.Get()};queue->ExecuteCommandLists(1,commands);check(swap->Present(tracing?0:1,0),"Present native frame");if(tracing)samples+=spp;timerValid=true;previousTracing=tracing;previousCamera=camera;cameraValid=true;historyValid=true;frameIndex++;
}
void Renderer::capture(const std::wstring&path){
 wait();auto desc=display->GetDesc();D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};UINT64 bytes;device->GetCopyableFootprints(&desc,0,1,0,&footprint,nullptr,nullptr,&bytes);auto readback=buffer(bytes,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_FLAG_NONE,D3D12_RESOURCE_STATE_COPY_DEST);allocator->Reset();list->Reset(allocator.Get(),nullptr);barrier(list.Get(),display.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COPY_SOURCE);D3D12_TEXTURE_COPY_LOCATION source{},destination{};source.pResource=display.Get();source.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;destination.pResource=readback.Get();destination.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;destination.PlacedFootprint=footprint;list->CopyTextureRegion(&destination,0,0,0,&source,nullptr);barrier(list.Get(),display.Get(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);list->Close();ID3D12CommandList*commands[]={list.Get()};queue->ExecuteCommandLists(1,commands);wait();
 ComPtr<IWICImagingFactory>factory;check(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&factory)),"Create PNG writer");ComPtr<IWICStream>stream;factory->CreateStream(&stream);check(stream->InitializeFromFilename(path.c_str(),GENERIC_WRITE),"Open image output");ComPtr<IWICBitmapEncoder>encoder;factory->CreateEncoder(GUID_ContainerFormatPng,nullptr,&encoder);encoder->Initialize(stream.Get(),WICBitmapEncoderNoCache);ComPtr<IWICBitmapFrameEncode>frame;encoder->CreateNewFrame(&frame,nullptr);frame->Initialize(nullptr);frame->SetSize(width,height);WICPixelFormatGUID format=GUID_WICPixelFormat32bppRGBA;check(frame->SetPixelFormat(&format),"Set PNG format");void*p;readback->Map(0,nullptr,&p);check(frame->WritePixels(height,footprint.Footprint.RowPitch,UINT(bytes),static_cast<BYTE*>(p)),"Write rendered PNG");readback->Unmap(0,nullptr);frame->Commit();encoder->Commit();
}
void Renderer::shutdown(){wait();if(uiEnabled){ImGui_ImplDX12_Shutdown();ImGui_ImplWin32_Shutdown();ImGui::DestroyContext();uiEnabled=false;}if(eventHandle){CloseHandle(eventHandle);eventHandle=nullptr;}}
