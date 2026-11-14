# Native Windows port

The native port is developed on the `windows-native-sdl3` branch. The initial
work keeps the original game sources intact while establishing a clean,
portable platform boundary.

## Architecture

- Game and renderer code remain C. The project uses C11; no C++ is required.
- SDL3 is an implementation detail of the platform layer.
- The game-facing platform API lives in `src/platform/platform.h`.
- SDL3 implementation details live in `src/platform/sdl3/`.
- The presentation contract is an 8-bit indexed framebuffer and a 256-entry
  RGB palette. The platform converts this to an SDL texture.
- Original game logic, resource formats and rendering algorithms should be
  retained wherever practical. DOS hardware access and compiler-specific
  assembly are replaced behind platform or renderer interfaces.

## Current implementation

The SDL3 adapter currently provides:

- Window creation and event pumping.
- Monotonic millisecond timing.
- Presentation of a caller-owned 8-bit indexed framebuffer and RGB palette.
- Nearest-neighbour scaling with letterboxing, preserving the framebuffer
  aspect ratio.
- A standalone smoke-test executable that exercises this path without
  depending on the legacy engine.

The framebuffer defaults to 320x200, but dimensions are configurable. The
platform copies each submitted frame, so the caller may reuse its framebuffer
as soon as presentation returns.

**Important:** this is a presentation layer, not yet the original game's video
backend. The original VGA code uses planar video memory and direct hardware
register access. That path must be replaced with a portable linear framebuffer
implementation before the game renderer can submit frames here.

## Build

Install SDL3 development files and make its CMake package discoverable, then:

```sh
cmake -S . -B build -DSDL3_DIR=/path/to/SDL3/cmake
cmake --build build --config Release
```

Run `wolf3d_platform_smoke` to verify the window and indexed-framebuffer
presentation path.

If SDL3 is not found, CMake reports that the platform targets are skipped.
This is deliberate while the legacy engine is being separated from its DOS
dependencies.

## Framebuffer test suite

The portable framebuffer library is built independently of SDL3, so its tests
can run on machines that do not have SDL installed.

Run locally:

```sh
cmake -S . -B build -DWOLF3D_BUILD_SDL3_PLATFORM=OFF
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

GitHub Actions runs the same CTest suite on both Ubuntu and Windows. The tests
cover framebuffer initialisation and storage bounds, row-pitch padding,
clipped pixel/line/rectangle operations, overlapping copies, planar VGA image
conversion, and expansion of the original 6-bit DAC palette to 8-bit RGB.

## Next steps

1. Replace the VGA register and segmented-memory operations with a portable
   software framebuffer, preserving the existing drawing API where possible.
2. Audit the original assembly drawing routines and translate their behaviour
   into readable C.
3. Connect the original renderer to the platform presentation API.
4. Port input, timing, audio, filesystem and memory management independently.
5. Bring up the original game loop only after the renderer and resource loading
   paths build cleanly on a modern compiler.

Do not copy SDL types into game headers. Do not introduce a second renderer
while the original software renderer can be preserved.
