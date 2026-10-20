#pragma once
#include <windows.h>
#include <d3d12.h>
#define XR_USE_PLATFORM_WIN32
#define XR_USE_GRAPHICS_API_D3D12
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include "vr_math.h"
#include <ostream>

class VrSession {
public:
 ~VrSession(){stop();}
 bool start(ID3D12Device*device,ID3D12CommandQueue*queue,std::ostream&log);
 void stop();bool begin();void end(bool rendered);void haptic(int hand);void resizePanel(uint32_t w,uint32_t h);
 bool active()const{return session!=XR_NULL_HANDLE;}bool running=false,focused=false,shouldRender=false,exitRequested=false;
 bool pausePressed=false,resetPressed=false,recenterPressed=false,menuPressed=false;
 bool triggerPressed[2]{},triggerHeld[2]{};VrPointer pointers[2];VrEye eyes[2];Camera listener;
 bool showControls=false,uiHover=false,uiDown=false;V2 uiPosition;int uiHand=-1;float uiScroll=0;
 VrPlacement placement;float resolutionScale=.7f;uint32_t width=0,height=0,renderWidth=0,renderHeight=0;float refreshHz=0;bool useSystemRuntime=false;
 std::string status="Connect Quest Link, then enter VR.",runtimeName;std::wstring audioDevice;
 ID3D12Resource*image=nullptr,*panelImage=nullptr;uint64_t submittedFrames=0;
 void requestRecenter(){placement.centered=false;}
private:
 XrInstance instance=XR_NULL_HANDLE;XrSystemId system=XR_NULL_SYSTEM_ID;XrSession session=XR_NULL_HANDLE;
 XrSpace local=XR_NULL_HANDLE,headSpace=XR_NULL_HANDLE,aimSpace[2]{};
 XrSwapchain swapchain=XR_NULL_HANDLE;XrActionSet actionSet=XR_NULL_HANDLE;
 XrSwapchain panelSwapchain=XR_NULL_HANDLE;std::vector<XrSwapchainImageD3D12KHR>panelImages;uint32_t panelWidth=0,panelHeight=0;bool panelAcquired=false,placePanel=true;XrPosef panelPose{{0,0,0,1},{0,0,0}};
 XrAction aim=XR_NULL_HANDLE,trigger=XR_NULL_HANDLE,pause=XR_NULL_HANDLE,reset=XR_NULL_HANDLE,recenter=XR_NULL_HANDLE,menu=XR_NULL_HANDLE,vibration=XR_NULL_HANDLE,scroll=XR_NULL_HANDLE;
 ID3D12CommandQueue*boundQueue=nullptr;bool runtimeOverride=false;
 XrPath hands[2]{};bool triggerDown[2]{};XrSessionState state=XR_SESSION_STATE_UNKNOWN;
 XrFrameState frame{XR_TYPE_FRAME_STATE};std::array<XrView,2>views{{{XR_TYPE_VIEW},{XR_TYPE_VIEW}}};
 std::vector<XrSwapchainImageD3D12KHR>images;bool frameBegun=false,acquired=false;std::ostream*log=nullptr;
 void check(XrResult result,const char*operation);XrPath path(const char*name);void actions();void input();
};
