# Native Windows SDL3 port

The native game is developed on the `windows-native-sdl3` branch. The goal is
to run the original Wolfenstein 3-D C game on current Windows systems using
SDL3, while preserving the original game logic, resource formats, game rules
and software-rendering approach. New native code is C11; C++ is not enabled.

## Current integration status

The SDL3 executable is wired to the original game startup and main loop:

- `src/platform/native_game_main.c` creates the SDL3 host window, owns the
  legacy video context, binds presentation and timing callbacks, and calls
  `WolfMain()`.
- `WOLFSRC/WL_MAIN.C` retains the original startup, menus, demos, game loop
  and shutdown paths.
- `WOLFSRC/WL_MENU.C` detects the installed Wolfenstein data edition before
  resource initialization. The default build checks retail `.WL6`, registered
  `.WL3`, and shareware `.WL1` data.
- `src/platform/sdl3/platform_sdl3.c` provides the SDL3 window, event,
  keyboard, mouse, timing, audio-stream and indexed-framebuffer presentation
  services behind `src/platform/platform.h`.

The native build is more than a platform smoke executable: the original game
sources are compiled into the native target and linked to the SDL3 launcher.
However, a complete game session with original game data has not yet been
verified in this environment. The repository does not contain the original
commercial game data, and CI deliberately tests the missing-data startup path
rather than distributing or downloading those files. A successful build and
missing-data check must not be treated as proof that the campaign is playable.

## Architecture

SDL3 remains an implementation detail of the platform layer. The original game
uses an 8-bit indexed framebuffer and a 256-entry palette; SDL3 converts each
frame to the renderer's ARGB texture format, uses nearest-neighbour scaling and
letterboxes the 320x200 logical image in a resizable window.

The software video path preserves the original four-plane VGA layout:

- Four 65,536-byte planes, with the original 80-byte plane stride and page
  offsets.
- Planar image conversion, latch storage, drawing primitives, palette
  expansion, fades and page-to-framebuffer presentation.
- A game-facing `VL_*` compatibility layer used by the original drawing and
  UI code.

The native renderer keeps the original wall and sprite drawing code in
`WOLFSRC/WL_DRAW.C`. The former DOS/assembly ray traversal is replaced by
`src/video/native_raycaster.c`; the generated x86 scaler is replaced by
readable C in `src/platform/native_scaling.c` and
`src/video/native_sprite_scaler.c`. Wall posts are drawn through the software
VGA memory model. This is intended to preserve the original renderer rather
than introduce a second rendering engine.

## Native runtime services

- **Memory:** `native_memory_manager.c` provides stable heap-backed
  allocations for the legacy memory-manager API.
- **VSWAP pages:** `native_page_manager.c` validates the original page-file
  header, offsets and lengths, loads pages lazily and keeps their addresses
  stable.
- **Maps:** `native_map_codec.c` provides bounded Carmack and RLEW decoding;
  map headers and file ranges are validated during startup.
- **Graphics:** the graphics archive path validates dictionary and offset
  tables, bounds compressed and expanded chunk sizes, and uses the native
  bounded Huffman decoder in `native_huffman.c`.
- **Input:** keyboard scan codes and mouse movement/buttons are mapped to the
  original input-manager interface. Joystick input is not implemented.
- **Sound effects:** PC-speaker command streams are synthesised into mono PCM
  and submitted through the SDL3 audio stream. The sound manager reports
  successful submissions to the original game code.

The current native sound implementation does not provide AdLib/FM music or
digitised sound playback. The legacy CRTC split-screen operation remains a
compatibility stub. These are known feature gaps; they do not replace the
original game loop or renderer.

## Game data

Original game data is intentionally not stored in this repository. Supply a
legitimate copy of the matching original game data in a directory and point
the executable at it with `--data-dir`, or start the executable with that
directory as its working directory.

For the default Wolfenstein 3-D build, edition detection checks `.WL6`,
`.WL3` and `.WL1` files. The directory must contain the complete matching
data set, not only one archive. For example, the retail set includes the
matching graphics dictionary/header/archive, map header/archive, VSWAP pages
and audio files.

Example:

```powershell
.\build\Release\wolf3d_native.exe --data-dir "C:\Games\Wolf3D"
```

Use `wolf3d_native.exe --help` to display the launcher options. The default
CMake target is the standard Wolfenstein 3-D build; Spear of Destiny and
Japanese variants are not enabled by the current default target.

## Build

Install SDL3 development files and make the SDL3 CMake package discoverable.
On Windows with vcpkg:

```powershell
vcpkg install sdl3:x64-windows

cmake -S . -B build `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_INSTALLATION_ROOT/scripts/buildsystems/vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-windows `
  -DWOLF3D_BUILD_LEGACY_GAME_OBJECTS=ON `
  -DCMAKE_BUILD_TYPE=Release

cmake --build build --config Release --parallel
```

The executable is `build\\Release\\wolf3d_native.exe`.

For the portable libraries and tests without SDL3:

```sh
cmake -S . -B build -DWOLF3D_BUILD_SDL3_PLATFORM=OFF
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

To compile the original gameplay sources without the SDL3 executable:

```sh
cmake -S . -B build-game \
  -DWOLF3D_BUILD_SDL3_PLATFORM=OFF \
  -DWOLF3D_BUILD_LEGACY_GAME_OBJECTS=ON
cmake --build build-game --target wolf3d_game_objects --config Release
```

## Automated verification

GitHub Actions builds and runs the portable tests on Ubuntu and Windows. The
Linux job also runs AddressSanitizer and UndefinedBehaviorSanitizer. The
Windows job installs SDL3 with vcpkg, builds the native executable and
framebuffer smoke test, runs the smoke test headlessly, exercises launcher
help and invalid/missing data-directory handling, verifies the expected
missing-data startup message, and compiles the original gameplay C sources.

The portable test suite covers indexed framebuffer clipping and copying,
VGA plane addressing and page offsets, legacy video and `VL_*` drawing,
palette fades, sprite scaling and wall occlusion, ray traversal, native input
and sound, memory/page management, map codecs, Huffman expansion and native
compatibility shims including file-search iteration and cleanup.

These checks establish build, subsystem and startup-path correctness. The
remaining acceptance step is to run with a complete matching game-data set,
exercise menus and level loading, play through a level, and fix any issues
found during that end-to-end session.

## Design constraints

Keep SDL types out of the original game headers. Keep new native code in C.
Prefer adapting the original rendering and resource paths over introducing
parallel systems. Do not commit commercial game data or user-specific data
files to the repository.
