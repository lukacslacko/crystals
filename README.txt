CRYSTAL TUMBLER / ROTATING METAL CUBE / NATIVE DXR

Run CrystalSlab.exe with all eight .cso shaders beside it.
This portable C++20 / DirectX 12 app runs offline on a DXR 1.1 capable GPU.

THE ROTATING CUBE

The container is a 10.4 x 10.4 x 10.4 cube, larger than the original 8 x 5 x 2.8
crystal slab. Twelve chamfered polished-metal rails and eight champagne-metal
corner blocks define its outline. Six rigid faces contain the pieces; by default
they are invisible. The four side walls can also be paper, stone or metal.
The two end walls stay invisible. The studio wall and floor stay stationary.

The original 3D Voronoi crystal arrangement is released into this larger volume
as independent rigid bodies. Gravity makes the crystals fall, collide and scatter.
A small outward release velocity helps separate the pieces. The cube begins
turning immediately at four degrees per second (one revolution in 90 seconds).
Its axis passes through the cube center, horizontally front-to-back, perpendicular
to the wall behind it. The box is elevated enough to clear the floor at every angle.

The six walls are Bullet kinematic rigid bodies, driven by the same transform as
the visible metal rails. Gravity remains vertical in world coordinates. Contacts
with the rotating walls lift, slide and tumble the crystals. The containment
check operates in the rotating cube's local coordinates; a small conservative
projection corrects residual numerical drift. It does not rotate the crystals
as a rigid group.

MATERIAL CONTROLS

Open Materials... in the main panel. Choose Flat crystal, Rubbery, Stony or
Papery, then edit the values independently:
  Interior transparency: 0 (opaque tinted solid) to 1 (clear tinted crystal).
    Intermediate settings mix diffuse/transmitted light and bulk absorption.
  Surface shine: 0 (rough) to 1 (polished). Rough transmission looks frosted.
  Surface reflectance: 0 to 1, measured at normal incidence. This artistic
    control is independent of IOR; about 0.04 gives ordinary glass reflection.
  Surface structure: 0 to 1, for the rubber, stone and paper textures.

Procedural bump and pigment variation use object-local coordinates, so textures
stay attached to the moving pieces. Presets change friction, restitution and
approximate modal tuning/damping too. Rubber and paper are still rigid bodies.

The four walls parallel to the rotation axis can be Invisible, Paper, Stone or
Metal, together or individually. Their names refer to their starting positions.
Paper and stone receive crystal caustics; metal reflects light paths. Opaque walls
block picking of hidden crystals. Collision boundaries remain on all six sides.
Wall finishes also change friction and contact sound; invisible walls sound like
glass. End-wall sound remains glass.

Changes apply live, preserve body poses, and are saved in MaterialSettings.ini.
Reset materials restores clear crystals and invisible walls. Stable optics uses
fixed three-direction quadrature for early rough lobes to avoid random motion
grain; this is a bounded approximation. Stochastic mode progressively samples
rough reflections and transmission. The photon atlas includes four extra tiles
for the rotating side walls, retaining the original floor/backdrop resolution.

AMBIENT GLASS AUDIO

Sound is enabled at 75% by default. The output gain is now another 5x higher
(+14 dB before peak limiting) than the previous audio-settings build.
Use the Contact sound checkbox or M to mute. The volume slider
controls only this app. Pausing stops new contact
excitation; the remaining short glass ticks fade away in roughly 20 ms.

Open Audio settings below the main volume slider. All changes apply live:
  Master volume: 0-100%, default 75%.
  Decay time: 5-500 ms, default 20 ms. Approximate time to fade by 40 dB;
    individual shapes and higher modes vary slightly. Longer values ring more.
  Impacts / Scraping / Wall sounds: independent levels from 0-300%.
  Pitch: -12 to +12 semitones, retaining the differences between crystal shapes.
  Scrape texture: 0.25-3x grain density, from separate ticks to a finer scrape.
  Onset spread: 0-8 ms, default 8 ms. Independently staggers impacts within a
    physics tick so simultaneous collisions do not all start on the same sample.
  Reset audio defaults: restores these settings, including the master level.

Preferences are saved automatically beside the executable in AudioSettings.ini
and restored on launch. Adjustments work while paused as well as while moving.

Each crystal has six damped, inharmonic resonant modes. The approximate tuning
uses its mesh volume, three axis extents and vertex count. Larger crystals
ring lower, while elongated/irregular shapes shift the spacing of their modes.
Contact position changes which modes are excited. The default geometry produces
fundamentals around 1625 to 2692 Hz, with quieter higher modes up to 11.5 kHz.
The amplitude time constants are about 2 to 4.5 ms: an isolated hit falls more
than 40 dB below its attack around 20 ms. These are dry glass taps, with no reverb.

With the default invisible finish, the six walls have glass-pane resonators,
with lower tuning (around 1.05 kHz) and equally short release. They stay visually
invisible and retain the same rigid collision boundaries. Closing speed and
collision impulse excite brief strikes in both contacting objects. A gentle
compression of strike strength keeps small taps audible. Repeated resting
contacts are suppressed.

Sliding drives irregular, quiet micro-impacts into the modes, modeling successive
surface asperities touching. Faster tangential motion produces more micro-impacts;
slip and contact impulse determine their strength. Random intervals and signed
impulses give a fine scraping texture. There is no continuous noise oscillator.
New micro-impacts stop as soon as sliding/contact stops. Contact pressure adds
damping. Nothing is driven by a melody or an always-playing ambient loop.
The first scrape grain also receives a random offset when sliding begins.

Stereo placement, gentle high-frequency rolloff and smooth volume changes shape
the mix. A stereo-linked peak limiter with 2 ms lookahead, about 40 ms gain recovery
and a -1.41 dBFS ceiling replaces the previous saturating waveshaper. This controls
dense pile-ups without flattening each waveform peak into harmonic distortion.
This is a simple
modal approximation, not a calibrated acoustic material or finite-element model.
The audio runs independently of rendering through Windows XAudio2 at 48 kHz.
Its bounded queue and three streaming buffers give roughly 50 ms buffering.
Sound generation requires no external audio assets or additional runtime download.

The audio validation passed resting-contact silence, shape-dependent tuning,
20 ms impact decay on every crystal and pane, speed-dependent scrape density,
scrape stopping, mute fade, finite output and dense-impact peak limiting. The
slowest measured release was -43.88 dB in the 20-25 ms window relative to the
first 4 ms. The release test disables onset spread, accounts for limiter latency
and uses a settled low master level to measure the resonance without limiting.
An exported 24-second physics recording contains 18 seconds of tumbling followed
by a pause. Its active RMS was -26.98 dBFS and its peak -1.41 dBFS at the default
level. The real physics drove both crystal and pane resonators, including 76,725
scrape micro-impacts. A native output-device check streamed 98,400 stereo frames
with zero dropped updates.
Checks also cover the decay/pitch controls, independent channel levels, saved
preferences, valid setting ranges, and maximum-level/maximum-decay stress output.
Sixteen same-tick impacts produced sixteen distinct rendered onsets, spanning
0.81-7.67 ms; setting onset spread to zero restores alignment for comparison.

GlassAmbient.wav is the same procedural synthesizer rendered from the simulation,
provided as an optional preview; the app synthesizes sound live. AudioValidation.txt
records the checks. To reproduce after building:

build\Release\AudioValidation.exe C:\path\GlassAmbient.wav --device-test

CONTROLS

Drop again / R: rebuild and release a fresh set of crystals; reset the cube angle.
Pause / Space: freeze both crystals and cube; sound tails fade. Resume continues both.
Cube rotation: -12 to +12 degrees per second. Zero stops the motor while gravity
and free-body physics continue. Negative values reverse its direction.
Left click: extract a crystal. Right drag: orbit. Wheel: zoom.
Tab: hide/show the controls. M: mute/unmute. F9: save a clean PNG photograph.
Cell count: 24 to 96, applied on release.
Index of refraction: 1.01 to 2.50, applied live to camera rays and caustics.
Bevel width: 0 to 0.18, applied on release to every original crystal edge,
including the hidden edges. This updates the mesh and collision hull, preserving
its pose. Small cells cap their local bevel width to remain valid solids.

LIGHT AND RENDERING

One spotlight remains above the front center, repositioned for the larger cube.
The camera is pulled back, and the wall/caustic receiver covers the larger scene.
Hardware ray queries compute glass refraction, Fresnel reflection, absorption,
internal reflection and three-band color dispersion. Photon paths form colored
caustics on the studio floor and wall.

Stable crystal optics remains enabled by default. It traces weighted reflection
and refraction branches deterministically, with bounded branch count and depth,
plus approximate diffuse indirect lighting. This removes random Fresnel sparkle.
Fine edges can still alias. The moving photon estimate uses 1,048,576 paths and
spatial filtering; pause to refine caustics with 33,554,432 photon paths and to
accumulate additional antialiasing samples. Disable Stable crystal optics under
Renderer settings to use the stochastic path tracer instead.

The application keeps rendering while the cube rotates. Pausing allows it to
converge and rest the GPU. The GPU time readout covers camera rendering and
reconstruction; physics, geometry updates and photon tracing add other work.

MATERIAL VALIDATION

The material build passes SceneValidation, AudioValidation and MaterialValidation.
Material checks cover settings persistence/ranges, transparency endpoints, mixed
wall visibility, picking occlusion, rigid-body pose preservation, friction/audio
changes, and object-local texture coordinates after ten seconds of rotation.
Native GPU captures were inspected for every preset, paper/stone/metal walls,
partial transparency, mirror reflectance, and the stochastic rendering mode.
A live mixed-wall run passed containment checks for 360 frames at 1440 x 900
in 7.05 seconds. Its final camera-rendering GPU time was 12.79 ms at 3 spp.

BASELINE MEASUREMENTS BEFORE MATERIAL CONTROLS

The RTX 5070 rendered the live 360-frame tumble test at 1440 x 900 in 5.78 seconds,
including geometry and lighting updates. Its final camera-rendering GPU time was
13.86 ms, with nine samples per frame. The test advances physics by 1/60 second
per frame and checks containment every frame.

Four physics cases passed: 56 and 96 cells, positive and negative rotation speeds,
and different bevels. The first case ran for 120 simulated seconds at 120 Hz,
exceeding a full revolution; the remaining cases ran for 15 seconds each.
Checks cover initial falling, 3D dispersion and rotation, all six moving boundaries,
rotation-axis alignment, full-cycle clearance above the studio floor, and pause.
The first case measured up to 4.90 units of depth displacement from initial cell
centers. Closed-mesh, Voronoi-volume, IOR and bevel-pose checks also passed.
See Physics.txt and Validation.json for recorded results.

COMMAND LINE

CrystalSlab.exe --capture C:\path\still.png --settle 10 --samples 1536 --width 1920 --height 1200
CrystalSlab.exe --capture C:\path\moving.png --dynamic-test --width 1440 --height 900
CrystalSlab.exe --capture C:\path\orbit.png --motion-test
CrystalSlab.exe --capture C:\path\long-test.png --selftest --samples 192

A still capture simulates ten seconds, pauses, and refines the result.
--settle sets that simulation time (0 to 180 seconds).
--dynamic-test renders 360 live physics frames. --selftest simulates another
120 seconds with a containment check at every step before capturing.
--stability-test alternates two camera views with history reset every frame and
also saves the first matching view as .first.png. --raw disables denoising.
--path-tracing selects the stochastic integrator. --spp fixes its sample batch.
--no-caustics disables photon caustics. --yaw sets the initial camera angle.
--material crystal|rubber|stone|paper selects a starting finish.
--transparency, --shine and --reflectance accept values from 0 to 1.
--walls invisible|paper|stone|metal sets all four side walls.
--mixed-walls selects paper, stone, metal and invisible on successive sides.
Put a material preset before its numeric overrides. Capture mode uses defaults
unless overridden; it does not load saved material preferences.
Logs are written beside the executable.

BUILD FROM SOURCE

The source archive includes the C++/HLSL app, Dear ImGui 1.90.9 and Bullet 3.25.
Use Visual Studio 2022 C++ tools, CMake 3.24+ and Windows SDK 10.0.26100.0 with DXC.

cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DDELIVERY=C:/path/to/app
cmake --build build --config Release --parallel
build\Release\SceneValidation.exe

Bullet is zlib licensed; Dear ImGui is MIT licensed. See THIRD-PARTY.txt.
Bullet source: https://github.com/bulletphysics/bullet3/tree/3.25
