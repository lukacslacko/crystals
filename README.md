# Crystals

A native Windows crystal tumbler built with C++20, DirectX 12 hardware ray tracing, Bullet physics, and procedural glass audio.

![Colored Voronoi crystals tumbling in a rotating metal cube](docs/preview.png)

## Features

- True 3D Voronoi crystals, with tinted absorption, configurable refraction, and bevels on every edge.
- Live material controls: interior transparency (0–1), shine, reflectance, and crystal, rubber, stone, or paper surface presets with adjustable texture strength.
- Hardware ray queries for reflection, refraction, internal reflection, and color dispersion; photon paths produce colored caustics on the wall and floor.
- A rotating metal cube with six collision walls. Its four side walls can independently be invisible, paper, stone, or metal. A checkbox renders invisible walls, including both end walls, as thin glass. Crystals fall, tumble, slide, and collide inside it.
- Stable crystal optics for motion, plus a stochastic path-tracing mode and progressive still-image refinement.
- Six approximate resonant modes per crystal and per glass wall. Collisions drive short taps; sliding drives irregular micro-impacts.
- Material presets also adjust contact friction, restitution, and modal sound. All shapes remain rigid, including paper and rubber finishes.
- Live audio controls for volume, decay, impact/scrape/wall levels, pitch, scrape density, and onset spread. Preferences persist between launches.

## Requirements

- Windows 10/11, x64, with a DirectX 12 GPU and driver supporting **DXR 1.1**.
- Visual Studio 2022 with **Desktop development with C++**.
- A Windows SDK containing the DirectX Shader Compiler (`dxc.exe`). Tested with SDK **10.0.26100.0**.
- CMake **3.24 or newer**.

The current implementation has been tested on an NVIDIA RTX 5070 and AMD Ryzen 9 9950X3D. It requires hardware ray-query support; there is no software rendering fallback. The sound engine uses Windows XAudio2 at 48 kHz stereo.

## Build and run

```powershell
git clone https://github.com/lukacslacko/crystals.git
cd crystals
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel
.\build\app\CrystalSlab.exe
```

CMake finds DXC in the selected Windows SDK or on `PATH`. If your SDK is installed elsewhere, pass `-DDXC="C:/path/to/dxc.exe"` when configuring. To choose another output folder, pass `-DDELIVERY="C:/path/to/app"`.

Keep all eight `.cso` shader files beside `CrystalSlab.exe`. The build copies the license, dependency notices, and detailed guide there too. Bullet 3.25 and Dear ImGui 1.90.9 are vendored; no dependency download is needed during the build.

## Controls

| Control | Action |
| --- | --- |
| Right drag / mouse wheel | Orbit / zoom |
| Left click a crystal | Remove it |
| **R** / Drop again | Release a fresh arrangement |
| **Space** | Pause/resume physics and cube rotation |
| **M** | Mute/unmute |
| **Tab** | Show/hide controls |
| **F9** | Save a clean PNG beside the executable |

The panel controls cell count, index of refraction, bevel width, cube rotation, optics, and GPU rendering budget. **Audio settings** opens the sound panel. The default decay is approximately 20 ms, and impacts are independently staggered within an 8 ms window to avoid synchronized starts. Audio preferences are stored in `AudioSettings.ini` beside the executable.

**Materials…** opens the material panel. Choose a surface preset, then independently edit transparency, shine, reflectance, and structure. Transparency 0 gives an opaque tinted solid; 1 gives clear tinted transmission. Lower shine broadens highlights and roughens transmission. Reflectance sets the normal-incidence reflection strength independently of the refraction index. The structure slider controls procedural bump and pigment variation attached to each moving piece.

Set all four side walls together or expand **Set walls individually**. The names refer to their positions before rotation. Material preferences persist in `MaterialSettings.ini`. Changes preserve the current arrangement. Paper and stone panels receive crystal caustics; metal panels reflect the light paths.

Enable **Thin glass on invisible walls** to show the invisible sides and both end walls as clear, 0.035-unit glass panes. They reflect and refract light at IOR 1.5. The checkbox preserves paper/stone/metal finishes, collision boundaries, and sounds; crystals remain clickable through the glass. It is off by default and saved with the material settings.

![Stony pieces inside the cube with metal side walls](docs/materials.png)

## Validation

```powershell
ctest --test-dir build -C Release --output-on-failure
```

The tests check Voronoi geometry, rotating-container physics, containment, modal decay, onset spreading, scrape behavior, audio controls, saved settings, peak limiting, material ranges, independently visible walls, and texture coordinates during rotation. They do not need a GPU or an audio output device. The audio test writes a procedural recording to `build/GlassAmbient.wav`.

To check the actual audio output device as well:

```powershell
.\build\Release\AudioValidation.exe .\build\GlassAmbient.wav --device-test
```

For a rendered still or a live GPU/physics test:

```powershell
.\build\app\CrystalSlab.exe --capture "$PWD/still.png" --settle 10 --samples 1536 --width 1920 --height 1200
.\build\app\CrystalSlab.exe --capture "$PWD/moving.png" --dynamic-test
.\build\app\CrystalSlab.exe --capture "$PWD/stone.png" --material stone --walls metal
```

Capture overrides also accept `--transparency`, `--shine`, and `--reflectance` (0–1), `--mixed-walls`, and `--glass-walls`. Put the preset before its overrides. Captures start from default material settings, independent of saved UI preferences.

See [the detailed guide](README.txt) for rendering modes, command-line options, implementation notes, and recorded development measurements. The fast optics mode uses bounded ray branching, a fixed three-direction approximation for early rough reflection/refraction lobes, and approximate diffuse indirect light. The stochastic mode samples rough scattering progressively. The audio uses a lightweight modal approximation rather than finite-element acoustics.

## License

The application is released under the [MIT license](LICENSE).

Bundled dependencies retain their own licenses: **Bullet 3.25** uses the zlib license and **Dear ImGui 1.90.9** uses the MIT license. Their original notices are retained in `vendor/` and [THIRD-PARTY.txt](THIRD-PARTY.txt).
