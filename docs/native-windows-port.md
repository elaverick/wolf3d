# Native Windows port

The native port is developed on the `windows-native-sdl3` branch. The original
game sources remain intact while portable subsystems are introduced alongside
them.

## Architecture

- Game and renderer code remain C11. No C++ is used.
- SDL3 is an implementation detail of the platform layer.
- The game-facing platform API lives in `src/platform/platform.h`.
- SDL3 implementation details live in `src/platform/sdl3/`.
- The platform presents an 8-bit indexed framebuffer with a 256-entry 8-bit
  RGB palette.
- Original game logic, resource formats and rendering algorithms should be
  retained wherever practical. DOS hardware access and compiler-specific
  assembly are replaced behind platform or renderer interfaces.

## Current implementation

The SDL3 adapter currently provides:

- Window creation and event pumping.
- Monotonic millisecond timing.
- Indexed framebuffer presentation through an SDL streaming texture.
- Nearest-neighbour scaling with letterboxing.
- A standalone smoke-test executable that does not depend on the legacy game.

The portable video library currently provides:

- A caller-owned row-major 8-bit indexed framebuffer with clipping-safe pixel,
  line, rectangle and overlapping-copy operations.
- In-place conversion of chunky image data into the original four-plane VGA
  image layout, and blitting of those images into a linear framebuffer.
- Expansion of the original 6-bit VGA DAC palette into 8-bit RGB.
- A software model of four VGA memory planes, including the original page
  offsets, latch storage, drawing primitives and conversion of a selected page
  into a linear indexed framebuffer.
- A C wall-post scaler that reproduces the original 16.16 fixed-point mapping.

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

## Build

Install SDL3 development files and make its CMake package discoverable, then:

```sh
cmake -S . -B build -DSDL3_DIR=/path/to/SDL3/cmake
cmake --build build --config Release
```

Run `wolf3d_platform_smoke` to verify the SDL window and indexed-framebuffer
presentation path.

## Automated tests

The portable video library and tests build without SDL3.

```sh
cmake -S . -B build -DWOLF3D_BUILD_SDL3_PLATFORM=OFF
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

GitHub Actions runs Release builds and CTest on Ubuntu and Windows, treating
compiler warnings as errors. The Ubuntu job also runs an AddressSanitizer and
UndefinedBehaviorSanitizer build. Tests cover framebuffer bounds and clipping,
overlapping copies, image layout conversion, VGA plane addressing, original
page offsets, latch copies, palette expansion and wall-post scaling against
the original fixed-point algorithm.

## Next steps

1. Connect the software VGA memory model to the original `VL_*` video API.
2. Replace the VGA register and segmented-memory operations in the legacy
   drawing code with calls into this compatibility layer.
3. Translate sprite scaling and remaining assembly drawing routines into
   readable C, preserving their original pixel output.
4. Connect the selected display page and palette to SDL3 presentation.
5. Port input, timing, audio, filesystem and memory management independently.
6. Bring up the original game loop only after the renderer and resource loading
   paths build cleanly on a modern compiler.

Do not copy SDL types into game headers. Do not introduce a second renderer
while the original software renderer can be preserved.
