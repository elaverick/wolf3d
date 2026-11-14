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
  image layout.
- Blitting of four-plane image data into a linear framebuffer.
- Expansion of the original 6-bit VGA DAC palette into 8-bit RGB.
- A software model of four VGA memory planes, including page offsets, latch
  storage, basic drawing primitives and conversion of a selected page into a
  linear indexed framebuffer.

The VGA memory model is a compatibility layer, not a new renderer. It allows
the port to preserve the original engine's planar memory assumptions while
removing port I/O, segmented pointers and inline assembly. The original
320-pixel display width uses 80 bytes per plane row; the original page and
latch offsets will be wired in when the legacy video manager is connected.

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

GitHub Actions runs the CTest suite on Ubuntu and Windows. Tests cover
framebuffer initialisation and storage bounds, row-pitch padding, clipping,
overlapping copies, chunky-to-planar conversion, planar image blits, palette
expansion, VGA plane addressing, page offsets, latch copies and presentation.

## Next steps

1. Connect the software VGA memory model to the original `VL_*` video API.
2. Replace the VGA register and segmented-memory operations in the legacy
   drawing code with calls into this compatibility layer.
3. Translate the assembly scaler and remaining drawing routines into readable
   C, preserving their original pixel output.
4. Connect the selected display page and palette to SDL3 presentation.
5. Port input, timing, audio, filesystem and memory management independently.
6. Bring up the original game loop only after the renderer and resource loading
   paths build cleanly on a modern compiler.

Do not copy SDL types into game headers. Do not introduce a second renderer
while the original software renderer can be preserved.
