#include "renderer.h"
#include "audio.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"
#include <shellapi.h>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <filesystem>
#include <sstream>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND,UINT,WPARAM,LPARAM);
static bool interactive=false;static uint32_t requestedWidth=0,requestedHeight=0;
static bool fullscreen=false;static WINDOWPLACEMENT windowedPlacement{sizeof(WINDOWPLACEMENT)};static LONG_PTR windowedStyle=0;
static void setFullscreen(HWND window,bool enabled){
 if(enabled==fullscreen)return;
 if(enabled){
  MONITORINFO monitor{sizeof(MONITORINFO)};if(!GetMonitorInfo(MonitorFromWindow(window,MONITOR_DEFAULTTONEAREST),&monitor))return;
  if(!GetWindowPlacement(window,&windowedPlacement))return;windowedStyle=GetWindowLongPtr(window,GWL_STYLE);
  SetWindowLongPtr(window,GWL_STYLE,(windowedStyle&~WS_OVERLAPPEDWINDOW)|WS_POPUP);
  SetWindowPos(window,HWND_TOP,monitor.rcMonitor.left,monitor.rcMonitor.top,monitor.rcMonitor.right-monitor.rcMonitor.left,monitor.rcMonitor.bottom-monitor.rcMonitor.top,SWP_FRAMECHANGED|SWP_NOOWNERZORDER);
 }else{
  SetWindowLongPtr(window,GWL_STYLE,windowedStyle);SetWindowPlacement(window,&windowedPlacement);
  SetWindowPos(window,nullptr,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_FRAMECHANGED|SWP_NOOWNERZORDER);
 }fullscreen=enabled;
}
static LRESULT CALLBACK WindowProc(HWND window,UINT message,WPARAM w,LPARAM l){
 if(interactive&&ImGui::GetCurrentContext()&&ImGui_ImplWin32_WndProcHandler(window,message,w,l))return true;
 switch(message){case WM_SIZE:if(w!=SIZE_MINIMIZED){requestedWidth=LOWORD(l);requestedHeight=HIWORD(l);}return 0;case WM_DESTROY:PostQuitMessage(0);return 0;case WM_SYSCOMMAND:if((w&0xfff0)==SC_KEYMENU)return 0;break;}
 return DefWindowProc(window,message,w,l);
}
static void styleUI(){
 ImGui::StyleColorsDark();auto&s=ImGui::GetStyle();s.WindowRounding=8;s.FrameRounding=4;s.GrabRounding=4;s.WindowPadding={22,20};s.FramePadding={10,7};s.ItemSpacing={10,10};s.WindowBorderSize=1;
 s.Colors[ImGuiCol_WindowBg]={.025f,.032f,.04f,.87f};s.Colors[ImGuiCol_Border]={.56f,.57f,.55f,.18f};s.Colors[ImGuiCol_Text]={.92f,.91f,.86f,1};s.Colors[ImGuiCol_TextDisabled]={.56f,.59f,.61f,1};s.Colors[ImGuiCol_Button]={.23f,.25f,.25f,1};s.Colors[ImGuiCol_ButtonHovered]={.36f,.36f,.31f,1};s.Colors[ImGuiCol_SliderGrab]={.77f,.65f,.43f,1};s.Colors[ImGuiCol_SliderGrabActive]={.92f,.79f,.54f,1};s.Colors[ImGuiCol_FrameBg]={.14f,.16f,.17f,1};s.Colors[ImGuiCol_CheckMark]={.87f,.73f,.46f,1};
 ImGui::GetIO().Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf",17);ImGui::GetIO().Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeuil.ttf",32);
}
static void audioLevel(const char*id,float&value,const char*format,float maximum=100){float percent=value*100;ImGui::SetNextItemWidth(-1);if(ImGui::SliderFloat(id,&percent,0,maximum,format))value=percent*.01f;}
static void audioPanel(Renderer&r,AudioSettings&settings,bool&open,bool saveFailed){
 if(!open)return;
 ImGui::SetNextWindowPos({float(r.width)-365,25},ImGuiCond_FirstUseEver);
 ImGui::SetNextWindowSize({340,0});ImGui::SetNextWindowSizeConstraints({340,0},{340,std::max(250.f,float(r.height)-75)});
 if(ImGui::Begin("Audio settings",&open,ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoResize)){
  ImGui::Checkbox("Sound enabled",&settings.enabled);audioLevel("##Master",settings.volume,"Master volume %.0f%%");
  ImGui::TextDisabled("DECAY TIME");ImGui::SetNextItemWidth(-1);ImGui::SliderFloat("##Decay",&settings.decayMs,5,500,"%.0f ms",ImGuiSliderFlags_Logarithmic);
  if(ImGui::IsItemHovered())ImGui::SetTooltip("Approximate time for a glass tap to fall by 40 dB.\n20 ms is dry and crisp; longer values ring more.");
  ImGui::TextDisabled("IMPACTS");audioLevel("##Impacts",settings.impacts,"%.0f%%",300);
  ImGui::TextDisabled("SCRAPING");audioLevel("##Scraping",settings.scraping,"%.0f%%",300);
  ImGui::TextDisabled("WALL SOUNDS");audioLevel("##Walls",settings.walls,"%.0f%%",300);
  ImGui::TextDisabled("PITCH");ImGui::SetNextItemWidth(-1);ImGui::SliderFloat("##Pitch",&settings.pitch,-12,12,"%+.1f semitones");
  ImGui::TextDisabled("SCRAPE TEXTURE");ImGui::SetNextItemWidth(-1);ImGui::SliderFloat("##Texture",&settings.texture,.25f,3,"Grain density %.2fx");
  if(ImGui::IsItemHovered())ImGui::SetTooltip("Lower: separate tiny ticks. Higher: a finer scrape.\nSliding speed also controls the grain rate.");
  ImGui::TextDisabled("ONSET SPREAD");ImGui::SetNextItemWidth(-1);ImGui::SliderFloat("##OnsetSpread",&settings.onsetSpreadMs,0,8,"%.1f ms");
  if(ImGui::IsItemHovered())ImGui::SetTooltip("Stagger simultaneous impacts by a random delay.\n8 ms disperses them across a physics tick; 0 aligns them.");
  ImGui::Spacing();if(ImGui::Button("Reset audio defaults",{-1,32}))settings=AudioSettings{};
  ImGui::TextWrapped(saveFailed?"Could not save settings beside the app.":"Changes apply live and are saved automatically.");
 }ImGui::End();
}
static void materialPanel(Renderer&r,CrystalScene&s,bool&open,bool saveFailed){
 if(!open)return;
 ImGui::SetNextWindowPos({float(r.width)-395,25},ImGuiCond_FirstUseEver);
 ImGui::SetNextWindowSize({370,0});ImGui::SetNextWindowSizeConstraints({370,0},{370,std::max(250.f,float(r.height)-75)});
 if(ImGui::Begin("Materials",&open,ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoResize)){
  auto&a=s.appearance;ImGui::TextDisabled("CRYSTAL PIECES");int preset=a.finish;ImGui::SetNextItemWidth(-1);
  if(ImGui::Combo("##Finish",&preset,"Flat crystal\0Rubbery\0Stony\0Papery\0"))a.preset(preset);
  ImGui::TextWrapped("Choose a starting finish, then adjust its properties.");
  ImGui::TextDisabled("INTERIOR TRANSPARENCY");ImGui::SetNextItemWidth(-1);ImGui::SliderFloat("##Transparency",&a.transparency,0,1,"%.2f");
  if(ImGui::IsItemHovered())ImGui::SetTooltip("0: opaque tinted solid. 1: clear tinted crystal.\nIntermediate values mix diffuse and transmitted light,\nwith additional absorption through the interior.");
  ImGui::TextDisabled("SURFACE SHINE");ImGui::SetNextItemWidth(-1);ImGui::SliderFloat("##Shine",&a.shine,0,1,"%.2f");
  if(ImGui::IsItemHovered())ImGui::SetTooltip("Low: broad, rough highlights / frosted transmission.\nHigh: polished highlights and sharp reflections.");
  ImGui::TextDisabled("SURFACE REFLECTANCE");ImGui::SetNextItemWidth(-1);ImGui::SliderFloat("##Reflectance",&a.reflectance,0,1,"%.2f");
  if(ImGui::IsItemHovered())ImGui::SetTooltip("Reflection strength when viewed straight on.\nAbout 0.04 is typical glass; 1 is mirror-like.\nThis artistic control is independent of the refraction index.");
  ImGui::TextDisabled("SURFACE STRUCTURE");ImGui::BeginDisabled(a.finish==0);ImGui::SetNextItemWidth(-1);ImGui::SliderFloat("##Structure",&a.structure,0,1,"%.2f");ImGui::EndDisabled();
  ImGui::Spacing();ImGui::Separator();ImGui::Spacing();ImGui::TextDisabled("FOUR SIDE WALLS");
  constexpr const char*choices="Invisible (glass sound)\0Paper\0Stone\0Metal\0";
  int all=a.walls[0];for(int wall:a.walls)if(wall!=all){all=-1;break;}
  ImGui::SetNextItemWidth(-1);if(ImGui::BeginCombo("##AllWalls",all<0?"Mixed wall finishes":all==0?"All: invisible":all==1?"All: paper":all==2?"All: stone":"All: metal")){
   const char*names[]={"All: invisible","All: paper","All: stone","All: metal"};for(int i=0;i<4;i++)if(ImGui::Selectable(names[i],all==i))a.walls.fill(i);ImGui::EndCombo();
  }
  if(ImGui::CollapsingHeader("Set walls individually")){
   const char*names[]={"Bottom at start","Top at start","Left at start","Right at start"};for(int i=0;i<4;i++){ImGui::TextDisabled("%s",names[i]);ImGui::PushID(i);ImGui::SetNextItemWidth(-1);ImGui::Combo("##Wall",&a.walls[i],choices);ImGui::PopID();}
  }
  ImGui::Checkbox("Thin glass on invisible walls",&a.glassWalls);
  if(ImGui::IsItemHovered())ImGui::SetTooltip("Render thin, clear glass with reflection and refraction.\nApplies to invisible side walls and both end walls.\nYou can still click crystals through the glass.");
  ImGui::TextWrapped("The four side walls rotate with the frame. The glass checkbox also shows both end walls. All walls remain rigid.");
  ImGui::Spacing();if(ImGui::Button("Reset materials",{-1,32}))a=MaterialSettings{};
  ImGui::TextWrapped(saveFailed?"Could not save materials beside the app.":"Changes apply live and are saved automatically.");
 }ImGui::End();
}
static void objectPanel(Renderer&r,CrystalScene&s,bool&open,bool&reform){
 if(!open)return;
 ImGui::SetNextWindowPos({float(r.width)-395,25},ImGuiCond_FirstUseEver);ImGui::SetNextWindowSize({370,0});
 ImGui::SetNextWindowSizeConstraints({370,0},{370,std::max(250.f,float(r.height)-75)});
 if(ImGui::Begin("Original object",&open,ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoResize)){
  ImGui::TextDisabled("SHAPE BEFORE FRACTURING");ImGui::SetNextItemWidth(-1);
  if(ImGui::Combo("##Shape",&s.objectShape,"Box\0Faceted ellipsoid\0Cylinder\0Octahedron\0"))reform=true;
  const char*names[]={"Width (X)","Height (Y)","Depth (Z)"};float*dimensions[]={&s.width,&s.height,&s.depth};
  for(int i=0;i<3;i++){ImGui::TextDisabled("%s",names[i]);ImGui::SetNextItemWidth(-1);ImGui::PushID(i);ImGui::SliderFloat("##Dimension",dimensions[i],.5f,9.8f,"%.2f units",ImGuiSliderFlags_AlwaysClamp);if(ImGui::IsItemDeactivatedAfterEdit())reform=true;ImGui::PopID();}
  ImGui::TextDisabled("Fitted size: %.2f x %.2f x %.2f",s.width*s.objectScale,s.height*s.objectScale,s.depth*s.objectScale);
  ImGui::TextWrapped("Changes rebuild the object resting on the bottom wall. The octahedron starts on a face. Oversized shapes scale down together to leave room to tumble.");
  ImGui::TextWrapped("Equal ellipsoid dimensions make a faceted sphere. The cylinder's axis runs along Y.");
  if(ImGui::Button("Restore original slab",{-1,32})){s.objectShape=0;s.width=8;s.height=5;s.depth=2.8f;reform=true;}
 }ImGui::End();
}
static void colorPanel(Renderer&r,MaterialSettings&a,bool&open,bool saveFailed){
 if(!open)return;
 ImGui::SetNextWindowPos({float(r.width)-395,25},ImGuiCond_FirstUseEver);ImGui::SetNextWindowSize({370,0});ImGui::SetNextWindowSizeConstraints({370,0},{370,std::max(250.f,float(r.height)-75)});
 if(ImGui::Begin("Colors & lighting",&open,ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoResize)){
  ImGui::TextDisabled("COLOR DISTRIBUTION");ImGui::SetNextItemWidth(-1);ImGui::Combo("##ColorMode",&a.colorMode,"Random solid pieces\0Directional gradient\0Radial gradient\0Three-axis color blend\0Flowing bands\0");
  ImGui::TextDisabled("PALETTE");ImGui::SetNextItemWidth(-1);
  if(ImGui::BeginCombo("##Palette",colorPaletteName(a.palette))){for(int i=0;i<ColorPaletteCount;i++)if(ImGui::Selectable(colorPaletteName(i),a.palette==i))a.palette=i;ImGui::EndCombo();}
  for(int i=0;i<ColorStopCount;i++){auto tint=colorPalette(a.palette)[i];ImGui::PushID(i);ImGui::ColorButton("##Swatch",{std::pow(tint[0],6/2.2f),std::pow(tint[1],6/2.2f),std::pow(tint[2],6/2.2f),1},ImGuiColorEditFlags_NoTooltip|ImGuiColorEditFlags_NoDragDrop,{30,24});ImGui::PopID();if(i+1<ColorStopCount)ImGui::SameLine(0,8);}
  ImGui::TextDisabled("COLOR INTENSITY");ImGui::SetNextItemWidth(-1);ImGui::SliderFloat("##ColorIntensity",&a.colorIntensity,0,2,"%.2fx");
  if(ImGui::IsItemHovered())ImGui::SetTooltip("0: neutral clear/white. 1: palette colors. 2: deeper color.\nChanges pigment and absorption through the crystal.");
  if(a.colorMode==0){if(ImGui::Button("Shuffle piece colors",{-1,32}))a.colorSeed=(a.colorSeed+1)%1000001;ImGui::TextWrapped("Each piece gets one palette color. Shuffling preserves the geometry and motion.");}
  else{
   ImGui::TextDisabled("GRADIENT SCALE");ImGui::SetNextItemWidth(-1);ImGui::SliderFloat("##GradientScale",&a.gradientScale,.25f,4,"%.2fx");
   ImGui::TextDisabled("GRADIENT OFFSET");ImGui::SetNextItemWidth(-1);ImGui::SliderFloat("##GradientOffset",&a.gradientOffset,0,1,"%.2f");
   if(a.colorMode==1){ImGui::TextDisabled("DIRECTION / AZIMUTH");ImGui::SetNextItemWidth(-1);ImGui::SliderFloat("##GradientYaw",&a.gradientYaw,-180,180,"%.0f degrees");ImGui::TextDisabled("DIRECTION / ELEVATION");ImGui::SetNextItemWidth(-1);ImGui::SliderFloat("##GradientTilt",&a.gradientTilt,-90,90,"%.0f degrees");}
   ImGui::TextWrapped("The original object is colored throughout its volume. Every fragment carries its part of that gradient as it tumbles.");
  }
  ImGui::Spacing();ImGui::Separator();ImGui::Spacing();ImGui::Checkbox("Ambient light",&a.ambientLight);ImGui::BeginDisabled(!a.ambientLight);
  ImGui::TextDisabled("AMBIENT BRIGHTNESS");ImGui::SetNextItemWidth(-1);ImGui::SliderFloat("##AmbientBrightness",&a.ambientBrightness,0,8,"%.2fx",ImGuiSliderFlags_Logarithmic);ImGui::EndDisabled();
  ImGui::TextWrapped("Soft fill and reflection light. 1x is the original studio level. Also works with a black background.");
  ImGui::Spacing();if(ImGui::Button("Reset colors and lighting",{-1,32})){MaterialSettings defaults;a.colorMode=defaults.colorMode;a.palette=defaults.palette;a.colorSeed=0;a.colorIntensity=1;a.gradientScale=1;a.gradientOffset=0;a.gradientYaw=defaults.gradientYaw;a.gradientTilt=defaults.gradientTilt;a.ambientLight=true;a.ambientBrightness=1;}
  ImGui::TextWrapped(saveFailed?"Could not save settings beside the app.":"Changes apply live and are saved automatically.");
 }ImGui::End();
}
static void hud(Renderer&r,CrystalScene&s,GlassAudio&audio,AudioSettings&settings,bool&showAudio,bool&showMaterials,bool&showObject,bool&showColors,bool&reform,bool&show,float fps,int hovered){
 ImGui::SetNextWindowPos({25,25},ImGuiCond_Always);ImGui::SetNextWindowSize({300,0});ImGui::SetNextWindowBgAlpha(.90f);
 ImGui::SetNextWindowSizeConstraints({300,0},{300,std::max(200.f,float(r.height)-110)});
 ImGui::Begin("Crystal controls",nullptr,ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_AlwaysAutoResize);
 ImGui::TextDisabled("MATERIAL STUDY   /   07");ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);ImGui::TextUnformatted("Crystal Tumbler");ImGui::PopFont();ImGui::TextDisabled("ROTATING CUBE / MATERIALS");ImGui::Spacing();ImGui::Separator();ImGui::Spacing();
 ImGui::TextWrapped("%s",r.adapterName.c_str());ImGui::TextDisabled("%.1f GPU ms   |   %.1f fps",r.gpuMs,fps);ImGui::TextDisabled("%d spp/frame   |   %d total",r.spp,r.samples);ImGui::TextDisabled(r.samples>=r.maxSamples?"Converged / GPU resting":r.moving?(r.stableOptics?"Motion / deterministic crystal optics":"Motion / temporal reconstruction"):(r.stableOptics?"Refining / crystal antialiasing":"Refining / progressive path tracing"));ImGui::Spacing();
 if(ImGui::Button("Reset object",{118,36}))reform=true;ImGui::SameLine();if(ImGui::Button(s.paused?"Resume":"Pause",{118,36}))s.paused=!s.paused;
 ImGui::Checkbox("Contact sound",&settings.enabled);audioLevel("##AmbientVolume",settings.volume,"Volume %.0f%%");if(ImGui::Button("Audio settings...",{-1,30})){showAudio=!showAudio;if(showAudio){showMaterials=false;showObject=false;showColors=false;}}if(!audio.available())ImGui::TextDisabled("%s",audio.status().c_str());
 if(ImGui::Button("Materials...",{-1,34})){showMaterials=!showMaterials;if(showMaterials){showAudio=false;showColors=false;showObject=false;}}
 if(ImGui::Button("Colors & lighting...",{-1,34})){showColors=!showColors;if(showColors){showAudio=false;showMaterials=false;showObject=false;}}
 ImGui::Checkbox("Float in blackness",&s.appearance.blackBackground);
 if(ImGui::IsItemHovered())ImGui::SetTooltip("Remove the studio floor and backdrop.\nAmbient light is controlled in Colors & lighting.");
 bool wantFullscreen=fullscreen;if(ImGui::Checkbox("Fullscreen (F11)",&wantFullscreen))setFullscreen(r.window,wantFullscreen);
 if(ImGui::Button("Original object...",{-1,32})){showObject=!showObject;if(showObject){showMaterials=false;showAudio=false;showColors=false;}}
 ImGui::TextDisabled("CELL COUNT");ImGui::SetNextItemWidth(-1);if(ImGui::SliderInt("##Cells",&s.density,1,96)&&!ImGui::IsItemActive())reform=true;if(ImGui::IsItemDeactivatedAfterEdit())reform=true;
 ImGui::TextDisabled("INDEX OF REFRACTION");ImGui::SetNextItemWidth(-1);if(ImGui::SliderFloat("##IOR",&s.ior,1.01f,2.5f,"%.3f")){s.updateOptics();r.setScene(s);}
 ImGui::TextDisabled("BEVEL WIDTH / ALL EDGES");ImGui::SetNextItemWidth(-1);ImGui::SliderFloat("##Bevel",&s.bevel,0,.18f,"%.3f");if(ImGui::IsItemDeactivatedAfterEdit()){s.updateBevel();r.setScene(s);}
 if(ImGui::IsItemHovered())ImGui::SetTooltip("Applied on release. Keeps the current arrangement.\nSmall cells limit the bevel to preserve their volume.");
 ImGui::TextDisabled("CUBE ROTATION");ImGui::SetNextItemWidth(-1);ImGui::SliderFloat("##Rotation",&s.rotationSpeed,-12,12,"%.1f deg / sec");ImGui::TextDisabled("Front-to-back axis / 10.4-unit cube");
 ImGui::Spacing();ImGui::Separator();ImGui::Spacing();
 ImGui::Checkbox("Antialiasing",&r.antialiasing);
 if(ImGui::IsItemHovered())ImGui::SetTooltip("Smooth high-contrast edges, including during motion.\nSpatial filtering adds no grain or history trails.\nPaused views also keep their progressive ray sampling.");
 if(ImGui::Checkbox("Color dispersion",&r.dispersion))r.setScene(s);if(ImGui::Checkbox("Crystal caustics",&r.caustics))r.reset(true);
 if(ImGui::CollapsingHeader("Renderer settings")){
  if(ImGui::Checkbox("Stable crystal optics",&r.stableOptics))r.reset(true);
  if(ImGui::IsItemHovered())ImGui::SetTooltip("Trace reflection and refraction together.\nDeterministic glass + photon caustics; approximate diffuse bounce light.\nDisable for stochastic path tracing.");
  if(ImGui::Checkbox("Motion denoising",&r.denoise))r.reset(true);ImGui::Checkbox("Automatic GPU budget",&r.automaticBudget);
  ImGui::TextDisabled("TARGET GPU TIME (ms)");ImGui::SetNextItemWidth(-1);ImGui::SliderFloat("##Budget",&r.gpuBudgetMs,6,30,"%.1f ms");
  ImGui::TextWrapped("Higher GPU time allows more samples and can reduce the frame rate.");
  if(!r.automaticBudget){int n=int(r.spp);ImGui::TextDisabled("SAMPLES PER FRAME");ImGui::SetNextItemWidth(-1);if(ImGui::SliderInt("##Samples",&n,3,192)){r.spp=std::max(3,n/3*3);r.reset();}}
  ImGui::TextDisabled("RAY BOUNCES");ImGui::SetNextItemWidth(-1);int bounce=int(r.maxBounces);if(ImGui::SliderInt("##Bounces",&bounce,8,48)){r.maxBounces=bounce;r.reset(true);}
 }
 ImGui::Spacing();ImGui::Text("%d / %d cells remain",int(s.cells.size())-s.extracted,int(s.cells.size()));
 ImGui::TextDisabled("%d extracted   /   %.1f degrees",s.extracted,s.boxAngle*57.29578f);if(hovered>=0)ImGui::Text("Cell %02d  -  click to extract",hovered+1);else ImGui::TextDisabled("Choose a crystal to extract");
 ImGui::End();
 ImGui::SetNextWindowPos({float(r.width)*.5f,float(r.height)-22},ImGuiCond_Always,{.5f,1});ImGui::SetNextWindowBgAlpha(.68f);ImGui::Begin("Navigation",nullptr,ImGuiWindowFlags_NoDecoration|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoInputs);ImGui::TextDisabled("RIGHT DRAG  orbit     MIDDLE DRAG  pan XY     WHEEL  zoom     HOME  center");ImGui::TextDisabled("R  reset     SPACE  pause     M  mute     F9  photo     F11  fullscreen     TAB  controls");ImGui::End();
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int){
 wchar_t exe[MAX_PATH];GetModuleFileName(nullptr,exe,MAX_PATH);std::wstring directory=std::filesystem::path(exe).parent_path().wstring()+L"\\";
 std::ofstream log(directory+L"CrystalSlab.log");std::wstring capturePath;int targetSamples=192,fixedSPP=0,initialCells=56,initialShape=0;V3 initialSize{8,5,2.8f};V2 initialPan;bool selftest=false,motionTest=false,dynamicTest=false,stabilityTest=false,noCaustics=false,raw=false,pathTracing=false,noAA=false;float initialYaw=.34f,settleTime=10;uint32_t width=1440,height=900;MaterialSettings captureMaterials;
 int argc;LPWSTR*argv=CommandLineToArgvW(GetCommandLineW(),&argc);for(int i=1;i<argc;i++){std::wstring a=argv[i];if(a==L"--capture"&&i+1<argc)capturePath=argv[++i];else if(a==L"--samples"&&i+1<argc)targetSamples=_wtoi(argv[++i]);else if(a==L"--width"&&i+1<argc)width=_wtoi(argv[++i]);else if(a==L"--height"&&i+1<argc)height=_wtoi(argv[++i]);else if(a==L"--spp"&&i+1<argc)fixedSPP=_wtoi(argv[++i]);else if(a==L"--yaw"&&i+1<argc)initialYaw=float(_wtof(argv[++i]));else if(a==L"--settle"&&i+1<argc)settleTime=float(_wtof(argv[++i]));else if(a==L"--material"&&i+1<argc){std::wstring name=argv[++i];captureMaterials.preset(name==L"rubber"?1:name==L"stone"?2:name==L"paper"?3:0);}else if(a==L"--transparency"&&i+1<argc)captureMaterials.transparency=float(_wtof(argv[++i]));else if(a==L"--shine"&&i+1<argc)captureMaterials.shine=float(_wtof(argv[++i]));else if(a==L"--reflectance"&&i+1<argc)captureMaterials.reflectance=float(_wtof(argv[++i]));else if(a==L"--walls"&&i+1<argc){std::wstring name=argv[++i];captureMaterials.walls.fill(name==L"paper"?1:name==L"stone"?2:name==L"metal"?3:0);}else if(a==L"--cells"&&i+1<argc)initialCells=_wtoi(argv[++i]);else if(a==L"--shape"&&i+1<argc){std::wstring name=argv[++i];initialShape=name==L"ellipsoid"?1:name==L"cylinder"?2:name==L"octahedron"?3:0;}else if(a==L"--size"&&i+3<argc){initialSize.x=float(_wtof(argv[++i]));initialSize.y=float(_wtof(argv[++i]));initialSize.z=float(_wtof(argv[++i]));}else if(a==L"--color-mode"&&i+1<argc){std::wstring name=argv[++i];captureMaterials.colorMode=name==L"linear"?1:name==L"radial"?2:name==L"3d"?3:name==L"flow"?4:0;}
else if(a==L"--palette"&&i+1<argc){std::wstring name=argv[++i];captureMaterials.palette=name==L"jewel"?1:name==L"ocean"?2:name==L"aurora"?3:name==L"ember"?4:name==L"earth"?5:name==L"spectrum"?6:0;}
else if(a==L"--color-intensity"&&i+1<argc)captureMaterials.colorIntensity=float(_wtof(argv[++i]));
else if(a==L"--color-seed"&&i+1<argc)captureMaterials.colorSeed=_wtoi(argv[++i]);
else if(a==L"--gradient-scale"&&i+1<argc)captureMaterials.gradientScale=float(_wtof(argv[++i]));
else if(a==L"--gradient-offset"&&i+1<argc)captureMaterials.gradientOffset=float(_wtof(argv[++i]));
else if(a==L"--gradient-yaw"&&i+1<argc)captureMaterials.gradientYaw=float(_wtof(argv[++i]));
else if(a==L"--gradient-tilt"&&i+1<argc)captureMaterials.gradientTilt=float(_wtof(argv[++i]));
else if(a==L"--ambient"&&i+1<argc){captureMaterials.ambientLight=true;captureMaterials.ambientBrightness=float(_wtof(argv[++i]));}
else if(a==L"--no-ambient")captureMaterials.ambientLight=false;
else if(a==L"--pan"&&i+2<argc){initialPan.x=float(_wtof(argv[++i]));initialPan.y=float(_wtof(argv[++i]));}
else if(a==L"--black-background")captureMaterials.blackBackground=true;else if(a==L"--no-aa")noAA=true;else if(a==L"--glass-walls")captureMaterials.glassWalls=true;else if(a==L"--mixed-walls")captureMaterials.walls={1,2,3,0};else if(a==L"--selftest")selftest=true;else if(a==L"--dynamic-test")dynamicTest=true;else if(a==L"--motion-test")motionTest=true;else if(a==L"--stability-test")stabilityTest=true;else if(a==L"--no-caustics")noCaustics=true;else if(a==L"--raw")raw=true;else if(a==L"--path-tracing")pathTracing=true;}LocalFree(argv);interactive=capturePath.empty();
 try{
  CoInitializeEx(nullptr,COINIT_MULTITHREADED);SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  WNDCLASS wc{};wc.hInstance=instance;wc.lpfnWndProc=WindowProc;wc.lpszClassName=L"CrystalNativeDXR";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);RegisterClass(&wc);RECT bounds{0,0,LONG(width),LONG(height)};AdjustWindowRect(&bounds,WS_OVERLAPPEDWINDOW,FALSE);
  HWND window=CreateWindow(wc.lpszClassName,L"Crystal Tumbler | Native DXR",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,bounds.right-bounds.left,bounds.bottom-bounds.top,nullptr,nullptr,instance,nullptr);
  if(!window)throw std::runtime_error("Could not create the native application window");
  auto materialSettingsPath=std::filesystem::path(directory)/L"MaterialSettings.ini";CrystalScene scene;scene.density=initialCells;scene.objectShape=initialShape;scene.width=initialSize.x;scene.height=initialSize.y;scene.depth=initialSize.z;scene.appearance=interactive?loadMaterialSettings(materialSettingsPath):sanitizeMaterialSettings(captureMaterials);scene.create();if(!scene.geometryValid())throw std::runtime_error("Invalid 3D Voronoi geometry");if(!interactive&&!dynamicTest){for(int step=0;step<int(std::clamp(settleTime,0.f,180.f)*120);step++){scene.step(1.f/120);if(!scene.boundsValid(.003f))throw std::runtime_error("Rotating cube containment failed");}scene.paused=true;}Camera camera;camera.yaw=initialYaw;camera.pan(initialPan.x,initialPan.y,float(height));Renderer renderer;renderer.antialiasing=!noAA;renderer.stableOptics=!pathTracing;renderer.caustics=!noCaustics;renderer.denoise=!raw;if(fixedSPP){renderer.spp=std::max(3,fixedSPP/3*3);renderer.automaticBudget=false;}if(!interactive)renderer.maxSamples=std::max(1536,targetSamples);renderer.init(window,width,height,directory,interactive);if(interactive){styleUI();ShowWindow(window,SW_SHOW);UpdateWindow(window);}renderer.setScene(scene);
  log<<"Adapter: "<<renderer.adapterName<<"\nBackend: DirectX 12 inline ray queries / DXR 1.1\nTriangles: "<<scene.triangles.size()<<"\nSpotlights: "<<scene.lights.size()<<"; photon paths: "<<renderer.photons<<"\n";log.flush();
  if(selftest){scene.paused=false;for(int i=0;i<14400;i++){scene.step(1.f/120);if(!scene.boundsValid(.003f))throw std::runtime_error("Rotating cube containment failed");}scene.paused=true;renderer.setScene(scene);log<<"Rotating containment test: PASS, 120 seconds\n";}
  auto audioSettingsPath=std::filesystem::path(directory)/L"AudioSettings.ini";AudioSettings audioSettings=loadAudioSettings(audioSettingsPath);bool showAudio=false,audioDirty=false,audioSaveFailed=false;auto audioChanged=std::chrono::steady_clock::now();bool showColors=false,showObject=false,showMaterials=false,materialDirty=false,materialSaveFailed=false;auto materialChanged=audioChanged;
  GlassAudio sound;uint32_t soundRevision=scene.revision;if(interactive){sound.setSettings(audioSettings);sound.start();sound.configure(describeSound(scene));scene.soundSink=[&](const SoundFrame&frame){sound.submit(frame);};log<<"Audio: "<<sound.status()<<"; volume "<<audioSettings.volume*100<<"%; decay "<<audioSettings.decayMs<<" ms\n";log.flush();}
  auto start=std::chrono::steady_clock::now(),previous=start;MSG message{};bool running=true,showUI=true;float fps=0;int hovered=-1;uint32_t seed=73192,testFrame=0;double gpuSum=0;
  while(running){
   while(PeekMessage(&message,nullptr,0,0,PM_REMOVE)){if(message.message==WM_QUIT)running=false;TranslateMessage(&message);DispatchMessage(&message);}if(!running)break;
   if(requestedWidth&&requestedHeight){renderer.resize(requestedWidth,requestedHeight);requestedWidth=requestedHeight=0;}
   auto now=std::chrono::steady_clock::now();float dt=std::chrono::duration<float>(now-previous).count();previous=now;fps=fps*.94f+(1/std::max(dt,.001f))*.06f;
   bool reform=false;
   if(interactive){
    ImGui_ImplDX12_NewFrame();ImGui_ImplWin32_NewFrame();ImGui::NewFrame();auto&io=ImGui::GetIO();auto previousAudioSettings=audioSettings;auto previousMaterials=scene.appearance;
    if(ImGui::IsKeyPressed(ImGuiKey_Tab))showUI=!showUI;if(ImGui::IsKeyPressed(ImGuiKey_F11,false))setFullscreen(window,!fullscreen);if(fullscreen&&!io.WantCaptureKeyboard&&ImGui::IsKeyPressed(ImGuiKey_Escape,false))setFullscreen(window,false);
    if(!io.WantCaptureKeyboard){if(ImGui::IsKeyPressed(ImGuiKey_Home)){camera.target=scene.boxCenter;renderer.reset();}if(ImGui::IsKeyPressed(ImGuiKey_R))reform=true;if(ImGui::IsKeyPressed(ImGuiKey_Space))scene.paused=!scene.paused;if(ImGui::IsKeyPressed(ImGuiKey_M))audioSettings.enabled=!audioSettings.enabled;}
    if(!io.WantCaptureMouse){
     if(ImGui::IsMouseDragging(1)){camera.yaw-=io.MouseDelta.x*.004f;camera.pitch=std::clamp(camera.pitch+io.MouseDelta.y*.004f,-.45f,.65f);renderer.reset();}
     if(ImGui::IsMouseDragging(2)){camera.pan(io.MouseDelta.x,io.MouseDelta.y,float(renderer.height));renderer.reset();}
     if(io.MouseWheel!=0){camera.distance=std::clamp(camera.distance-io.MouseWheel*.9f,12.f,55.f);renderer.reset();}
     float x=io.MousePos.x/renderer.width*2-1,y=1-io.MousePos.y/renderer.height*2;hovered=scene.pick(camera.origin(),camera.ray(x,y,float(renderer.width)/renderer.height));
     if(ImGui::IsMouseClicked(0)&&scene.extract(hovered))renderer.setScene(scene);
    }else hovered=-1;
    if(showUI){hud(renderer,scene,sound,audioSettings,showAudio,showMaterials,showObject,showColors,reform,showUI,fps,hovered);audioPanel(renderer,audioSettings,showAudio,audioSaveFailed);materialPanel(renderer,scene,showMaterials,materialSaveFailed);objectPanel(renderer,scene,showObject,reform);colorPanel(renderer,scene.appearance,showColors,materialSaveFailed);}renderer.previewEdit=ImGui::IsAnyItemActive();
    if(scene.appearance!=previousMaterials){scene.updateMaterials();renderer.setScene(scene);materialDirty=true;materialChanged=now;}
    if(materialDirty&&!ImGui::IsAnyItemActive()&&std::chrono::duration<float>(now-materialChanged).count()>.3f){materialSaveFailed=!saveMaterialSettings(materialSettingsPath,scene.appearance);materialDirty=false;}
    if(audioSettings!=previousAudioSettings){sound.setSettings(audioSettings);audioDirty=true;audioChanged=now;}
    if(audioDirty&&!ImGui::IsAnyItemActive()&&std::chrono::duration<float>(now-audioChanged).count()>.3f){audioSaveFailed=!saveAudioSettings(audioSettingsPath,audioSettings);audioDirty=false;}
    ImGui::Render();
   }
   if(reform){scene.create(++seed);renderer.setScene(scene);}
   if(interactive&&scene.revision!=soundRevision){sound.configure(describeSound(scene));soundRevision=scene.revision;}
   if(scene.step(dynamicTest?1.f/60:dt)){if(dynamicTest&&!scene.boundsValid(.003f))throw std::runtime_error("Live rotating containment failed");renderer.setScene(scene);}
   if(motionTest){camera.yaw=.10f+.32f*float(std::min(testFrame,59u))/59;camera.pitch=.11f;renderer.reset();}
   if(stabilityTest){camera.yaw=initialYaw+.08f*float(testFrame%2);renderer.reset(true);}
   renderer.render(scene,camera,interactive);
   testFrame++;gpuSum+=renderer.gpuMs;if(interactive&&testFrame%300==0){log<<"Audio stream: "<<sound.status()<<"; frames "<<sound.renderedFrames()<<"; dropped "<<sound.droppedFrames()<<"\n";log.flush();}if(stabilityTest&&testFrame==1)renderer.capture(capturePath+L".first.png");
   if(interactive&&ImGui::IsKeyPressed(ImGuiKey_F9)){auto time=std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());std::wstringstream name;name<<directory<<L"Crystal-"<<time<<L".png";renderer.capture(name.str());}
   if(!interactive&&((stabilityTest&&testFrame>=61)||((motionTest&&testFrame>=60)||(dynamicTest&&testFrame>=360))||(!stabilityTest&&!motionTest&&!dynamicTest&&int(renderer.samples)>=targetSamples))){if(!scene.boundsValid())throw std::runtime_error("Physics containment test failed");renderer.capture(capturePath);log<<"Rendered "<<renderer.samples<<" samples at "<<width<<"x"<<height<<" in "<<std::chrono::duration<float>(std::chrono::steady_clock::now()-start).count()<<" seconds\nGPU compute: "<<renderer.gpuMs<<" ms; mean "<<gpuSum/testFrame<<" ms; final batch "<<renderer.spp<<" spp\nMotion test: "<<(motionTest?"YES":"NO")<<"; dynamic test: "<<(dynamicTest?"PASS":"NO")<<"\n";break;}
  }
  if(interactive&&audioDirty)saveAudioSettings(audioSettingsPath,audioSettings);if(interactive&&materialDirty)saveMaterialSettings(materialSettingsPath,scene.appearance);
  scene.soundSink={};log<<"Audio frames submitted: "<<sound.renderedFrames()<<"; dropped updates: "<<sound.droppedFrames()<<"\n";sound.stop();renderer.shutdown();DestroyWindow(window);CoUninitialize();return 0;
 }catch(const std::exception&e){log<<"ERROR: "<<e.what()<<"\n";log.flush();if(interactive)MessageBoxA(nullptr,e.what(),"Crystal Slab",MB_ICONERROR);return 1;}
}
