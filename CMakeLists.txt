cmake_minimum_required(VERSION 3.24)
project(CrystalSlab LANGUAGES CXX)
if(NOT WIN32 OR NOT MSVC)
  message(FATAL_ERROR "Crystal Tumbler requires Windows and the MSVC C++ toolchain.")
endif()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(DYNAMIC_LOADER OFF CACHE BOOL "" FORCE)
set(BUILD_API_LAYERS OFF CACHE BOOL "" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_CONFORMANCE_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_SDK_TESTS OFF CACHE BOOL "" FORCE)
set(BUILD_WITH_SYSTEM_JSONCPP OFF CACHE BOOL "" FORCE)
add_subdirectory(vendor/openxr-1.1.54 EXCLUDE_FROM_ALL)
set_target_properties(openxr_loader PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(BULLET_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/vendor/bullet-3.25/src)
foreach(LIB LinearMath BulletCollision BulletDynamics)
  file(GLOB_RECURSE BULLET_SOURCES CONFIGURE_DEPENDS ${BULLET_ROOT}/${LIB}/*.cpp)
  add_library(${LIB} STATIC ${BULLET_SOURCES})
  target_include_directories(${LIB} PUBLIC ${BULLET_ROOT})
  target_compile_options(${LIB} PRIVATE /O2 /W0)
endforeach()
target_link_libraries(BulletCollision PUBLIC LinearMath)
target_link_libraries(BulletDynamics PUBLIC BulletCollision LinearMath)
set(IMGUI vendor/imgui-1.90.9)
add_executable(CrystalSlab WIN32 main.cpp scene.cpp renderer.cpp vr.cpp sound_model.cpp audio.cpp material_settings.cpp
  ${IMGUI}/imgui.cpp ${IMGUI}/imgui_draw.cpp ${IMGUI}/imgui_tables.cpp ${IMGUI}/imgui_widgets.cpp
  ${IMGUI}/backends/imgui_impl_win32.cpp ${IMGUI}/backends/imgui_impl_dx12.cpp)
target_include_directories(CrystalSlab PRIVATE ${IMGUI} ${IMGUI}/backends)
target_compile_definitions(CrystalSlab PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
target_compile_options(CrystalSlab PRIVATE /O2 /EHsc /W3)
target_link_libraries(CrystalSlab PRIVATE BulletDynamics OpenXR::openxr_loader d3d12 dxgi d3dcompiler windowscodecs ole32 shell32 xaudio2)
add_executable(SceneValidation scene_validation.cpp scene.cpp material_settings.cpp)
target_compile_definitions(SceneValidation PRIVATE NOMINMAX)
target_compile_options(SceneValidation PRIVATE /O2 /EHsc)
target_link_libraries(SceneValidation PRIVATE BulletDynamics)
add_executable(AudioValidation audio_validation.cpp scene.cpp sound_model.cpp audio.cpp material_settings.cpp)
target_compile_definitions(AudioValidation PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_compile_options(AudioValidation PRIVATE /O2 /EHsc /W3)
target_link_libraries(AudioValidation PRIVATE BulletDynamics xaudio2 ole32)
add_executable(MaterialValidation material_validation.cpp material_settings.cpp scene.cpp sound_model.cpp)
target_compile_definitions(MaterialValidation PRIVATE NOMINMAX)
target_compile_options(MaterialValidation PRIVATE /O2 /EHsc /W3)
target_link_libraries(MaterialValidation PRIVATE BulletDynamics)
set(DELIVERY "${CMAKE_BINARY_DIR}/app" CACHE PATH "Executable and shader output directory")
set_target_properties(CrystalSlab PROPERTIES RUNTIME_OUTPUT_DIRECTORY_RELEASE "${DELIVERY}")
find_program(DXC NAMES dxc.exe HINTS
  "$ENV{WindowsSdkVerBinPath}/x64"
  "$ENV{WindowsSdkDir}/bin/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/x64"
  "C:/Program Files (x86)/Windows Kits/10/bin/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/x64"
  DOC "DirectX Shader Compiler (DXC) from the Windows SDK" REQUIRED)
foreach(ENTRY PathTrace Resolve Antialias Mirror ClearPhotons PhotonTrace PhotonHorizontal PhotonVertical Temporal Spatial)
  add_custom_command(OUTPUT "${DELIVERY}/${ENTRY}.cso"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${DELIVERY}"
    COMMAND "${DXC}" -T cs_6_5 -E ${ENTRY} -O3 -Fo "${DELIVERY}/${ENTRY}.cso" "${CMAKE_CURRENT_SOURCE_DIR}/pathtrace.hlsl"
    DEPENDS pathtrace.hlsl VERBATIM)
  list(APPEND SHADERS "${DELIVERY}/${ENTRY}.cso")
endforeach()
add_custom_target(Shaders ALL DEPENDS ${SHADERS})
add_dependencies(CrystalSlab Shaders)
add_custom_command(TARGET CrystalSlab POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
    "${CMAKE_CURRENT_SOURCE_DIR}/THIRD-PARTY.txt"
    "${CMAKE_CURRENT_SOURCE_DIR}/README.txt"
    "${CMAKE_CURRENT_SOURCE_DIR}/VR.txt"
    "${DELIVERY}" VERBATIM)

enable_testing()
add_executable(VrValidation vr_validation.cpp)
target_compile_definitions(VrValidation PRIVATE NOMINMAX)
target_link_libraries(VrValidation PRIVATE BulletDynamics)
add_test(NAME vr_validation COMMAND VrValidation)
add_test(NAME scene_validation COMMAND SceneValidation)
add_test(NAME audio_validation COMMAND AudioValidation "${CMAKE_CURRENT_BINARY_DIR}/GlassAmbient.wav")
add_test(NAME material_validation COMMAND MaterialValidation)
