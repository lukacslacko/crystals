#pragma once
#include <array>
#include <filesystem>

enum class CrystalFinish { Crystal, Rubber, Stone, Paper };
enum class WallFinish { Invisible, Paper, Stone, Metal };
inline constexpr int ColorPaletteCount=7,ColorStopCount=8;
using ColorPalette=std::array<std::array<float,3>,ColorStopCount>;
const ColorPalette& colorPalette(int index);
const char* colorPaletteName(int index);
struct MaterialSettings {
 int finish=0;
 float transparency=1,shine=1,reflectance=.045f,structure=.65f;
 std::array<int,4>walls{}; // -Y, +Y, -X, +X; the Z end walls use the invisible finish.
 bool glassWalls=false;
 bool blackBackground=false;
 int colorMode=0,palette=0,colorSeed=0; // Random pieces, directional, radial, 3D corners, flowing bands.
 float colorIntensity=1,gradientScale=1,gradientOffset=0,gradientYaw=45,gradientTilt=35.26439f;
 bool ambientLight=true;
 float ambientBrightness=1;
 bool operator==(const MaterialSettings&)const=default;
 void preset(int index);
};
MaterialSettings sanitizeMaterialSettings(MaterialSettings settings);
MaterialSettings loadMaterialSettings(const std::filesystem::path&path);
bool saveMaterialSettings(const std::filesystem::path&path,const MaterialSettings&settings);
