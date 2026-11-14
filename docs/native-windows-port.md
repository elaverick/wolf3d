# Native Windows port

The native port is developed on the `windows-native-sdl3` branch. The original
game sources remain intact while portable subsystems are introduced alongside
them. All new code and the native build use C11; C++ is not enabled in CMake.

## Architecture

- SDL3 is an implementation detail of the platform layer.
- The game-facing platform API lives in `src/platform/platform.h`.
- SDL3 implementation details live in `src/platform/sdl3/`.
- The platform presents an 8-bit indexed framebuffer with a 256-entry 8-bit
  RGB palette.
- `WOLFSRC/ID_HEADS.H` selects `ID_HEADS_NATIVE.H` for native builds.
- `WOLFSRC/ID_VL.H` selects the portable `src/video/legacy_vl.c`
  implementation for native builds.
- `src/platform/native_compat.c` supplies narrowly scoped compatibility for
  legacy CRT calls, DOS-style file enumeration and the old mouse-register API.
  The SDL3 platform provides the actual mouse state and window-coordinate
  conversion.

Original game logic, resource formats and rendering algorithms should be
retained wherever practical. DOS hardware access and compiler-specific
assembly are replaced behind platform or renderer interfaces.

## Current implementation

The SDL3 adapter provides window creation, event pumping, monotonic timing,
millisecond delays, mouse input and indexed framebuffer presentation through a
streaming texture. It scales with nearest-neighbour filtering and letterboxing.

The portable video library provides:

- A caller-owned row-major 8-bit indexed framebuffer with clipping-safe pixel,
  line, rectangle and overlapping-copy operations.
- In-place conversion of chunky image data into the original four-plane VGA
  image layout, and blitting of those images into a linear framebuffer.
- Expansion of the original 6-bit VGA DAC palette into 8-bit RGB.
- A software model of four VGA memory planes, including the original page
  offsets, latch storage, drawing primitives and conversion of a selected page
  into a linear indexed framebuffer.
- A C wall-post scaler that reproduces the original 16.16 fixed-point mapping.
- A game-facing legacy video context that groups the planes, current drawing
  and display pages, palette, fades and linear presentation buffer.
- A portable `VL_*` compatibility layer for page selection, palette
  operations, fades, drawing primitives, image/latch copies and tile/latch
  string drawing.

The VGA memory model is a compatibility layer, not a new renderer. It preserves
the original planar memory model while removing port I/O, segmented pointers
and inline assembly.

The original VGA layout is represented explicitly:

| Item | Value |
|---|---:|
| Plane size | 65,536 bytes |
| Plane row stride | 80 bytes |
| Screen page size | 16,640 bytes (80 x 208) |
| Page 1 offset | 0 |
| Page 2 offset | 16,640 |
| Page 3 offset | 33,280 |
| Latch storage starts | 49,920 |

The image and latch copy functions currently require four-pixel-aligned
destination X coordinates, matching the original picture and latch drawing
paths. Pixel, line and rectangle operations accept pixel-aligned coordinates.
The CRTC split-screen operation remains a compatibility stub; it will need a
software equivalent if the game uses that path in the native build.

The SDL smoke executable exercises the portable `VL_*` API through the
platform presentation callback. It is separate from the original game loop.

## Original game source integration

The native header path is now in place, and the opt-in
`wolf3d_game_objects` CMake target compiles this initial group of original
gameplay modules as C:

- `WL_ACT1.C`, `WL_ACT2.C`, `WL_AGENT.C`
- `WL_GAME.C`, `WL_INTER.C`, `WL_MENU.C`
- `WL_PLAY.C`, `WL_STATE.C`, `WL_TEXT.C`

The menu callbacks have typed adapters rather than incompatible function
pointer casts. The old mouse interrupt calls are routed through the platform
input layer, and the native CRT shim provides portable replacements for the
legacy conversion and file-search functions.

This is a compile milestone, not a playable game. The renderer, generated
scalers, DOS managers, resource loading, audio and original main loop have not
yet been brought up as one executable.

## Build

For the portable video library and tests without SDL3:

```sh
cmake -S . -B build -DWOLF3D_BUILD_SDL3_PLATFORM=OFF
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

To compile the selected original gameplay sources as C:

```sh
cmake -S . -B build-game \
  -DWOLF3D_BUILD_SDL3_PLATFORM=OFF \
  -DWOLF3D_BUILD_LEGACY_GAME_OBJECTS=ON
cmake --build build-game --target wolf3d_game_objects --config Release
```

For the SDL3 platform build, install SDL3 development files and make its CMake
package discoverable:

```sh
cmake -S . -B build -DSDL3_DIR=/path/to/SDL3/cmake
cmake --build build --config Release
```

Run `wolf3d_platform_smoke` to verify the SDL window and indexed-framebuffer
presentation path.

## Automated tests

GitHub Actions runs Release builds and CTest on Ubuntu and Windows, treating
warnings as errors for the portable libraries and test harness. The Ubuntu job
also runs AddressSanitizer and UndefinedBehaviorSanitizer. The Windows job
installs SDL3 with vcpkg, builds the SDL platform and smoke executable, and
compiles the selected original gameplay sources as C.

Tests cover framebuffer bounds and clipping, overlapping copies, image layout
conversion, VGA plane addressing, original page offsets, latch copies, palette
expansion, wall-post scaling against the original fixed-point algorithm,
palette fades, the legacy `VL_*` API and native compatibility shims.

## Next steps

1. Port the original VGA drawing and generated-scaler paths in `WL_DRAW.C`
   and `WL_SCALE.C` to readable C, preserving their pixel output.
2. Replace DOS memory, resource, audio and timing managers with portable
   implementations, retaining their public game-facing APIs.
3. Connect the original wall and sprite renderer to the indexed framebuffer
   and SDL3 presentation callback.
4. Port the DOS startup, configuration and main-loop paths, then build and run
   the original game end to end.

Do not copy SDL types into game headers. Do not introduce a second renderer
while the original software renderer can be preserved.
