#pragma once
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <string>
#include <chrono>
#include "scene.h"
using Microsoft::WRL::ComPtr;
struct alignas(256) GPUConstants {
 F4 origin,forward,right,up,settings;U4 counts,options;
 F4 lightPosition[4],lightDirection[4],lightColor[4];
 F4 previousOrigin,previousForward,previousRight,previousUp,renderParams,sceneParams;
 F4 palette[ColorStopCount],colorSettings,gradientDirection,sourceScale;
};
class Renderer {
public:
 HWND window=nullptr;uint32_t width=1280,height=800,samples=0,spp=3,maxBounces=24,maxSamples=1536;
 bool denoise=true,dispersion=true,caustics=true,antialiasing=true,automaticBudget=true,moving=false,stableOptics=true,previewEdit=false;
 float gpuMs=0,gpuBudgetMs=12.5f;uint32_t photons=33554432;std::string adapterName;
 ComPtr<ID3D12Device5> device;ComPtr<ID3D12GraphicsCommandList4> list;ComPtr<ID3D12DescriptorHeap> heap;
 void init(HWND hwnd,uint32_t w,uint32_t h,const std::wstring&directory,bool ui);
 void resize(uint32_t w,uint32_t h);void setScene(const CrystalScene&scene);void render(const CrystalScene&scene,const Camera&camera,bool drawUI);void reset(bool invalidateHistory=false){samples=0;if(invalidateHistory)historyValid=false;}
 void capture(const std::wstring&path);void wait();void shutdown();
private:
 ComPtr<IDXGISwapChain3> swap;ComPtr<ID3D12CommandQueue> queue;ComPtr<ID3D12CommandAllocator> allocator;ComPtr<ID3D12Fence> fence;HANDLE eventHandle=nullptr;UINT64 fenceValue=0;
 ComPtr<ID3D12DescriptorHeap> rtvHeap;ComPtr<ID3D12Resource>backbuffers[2];UINT rtvSize=0;
 ComPtr<ID3D12Resource>accum,display,guide,photonMap,historyColor,historyGuide,filtered,denoiseTemp,photonTemp,photonFiltered,resolved;
 ComPtr<ID3D12Resource>triBuffer,vertexBuffer,materialBuffer,constantBuffer,blas,tlas,scratch,instances;
 ComPtr<ID3D12RootSignature>root;ComPtr<ID3D12PipelineState>pathPSO,resolvePSO,antialiasPSO,clearPhotonsPSO,photonPSO,temporalPSO,spatialPSO,photonHPSO,photonVPSO;
 ComPtr<ID3D12QueryHeap>timerHeap;ComPtr<ID3D12Resource>timerReadback;UINT64 timestampFrequency=0;
 bool timerValid=false,previousTracing=false,historyValid=false,cameraValid=false,photonsDirty=true,lowQualityPhotons=false;
 Camera previousCamera;uint32_t frameIndex=0;std::chrono::steady_clock::time_point lastMotion=std::chrono::steady_clock::now(),lastSceneChange=lastMotion;
 size_t triangleCount=0;bool dirty=true,uiEnabled=false;UINT64 scratchSize=0;
 void textures();void buildAS();
 ComPtr<ID3D12Resource> buffer(UINT64 bytes,D3D12_HEAP_TYPE type,D3D12_RESOURCE_FLAGS flags=D3D12_RESOURCE_FLAG_NONE,D3D12_RESOURCE_STATES state=D3D12_RESOURCE_STATE_GENERIC_READ);
};
