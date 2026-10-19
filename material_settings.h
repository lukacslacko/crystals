#pragma once
#include <array>
#include <filesystem>

enum class CrystalFinish { Crystal, Rubber, Stone, Paper };
enum class WallFinish { Invisible, Paper, Stone, Metal };
struct MaterialSettings {
 int finish=0;
 float transparency=1,shine=1,reflectance=.045f,structure=.65f;
 std::array<int,4>walls{}; // -Y, +Y, -X, +X; the two Z end walls stay invisible.
 bool operator==(const MaterialSettings&)const=default;
 void preset(int index);
};
MaterialSettings sanitizeMaterialSettings(MaterialSettings settings);
MaterialSettings loadMaterialSettings(const std::filesystem::path&path);
bool saveMaterialSettings(const std::filesystem::path&path,const MaterialSettings&settings);
