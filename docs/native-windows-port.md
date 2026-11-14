# Native Windows port

## Direction

The native port uses C11 and SDL3. The original game code remains the authority
for gameplay, simulation, resource formats and rendering behaviour.

SDL3 is a platform backend, not a replacement game engine. Operating-system
services are exposed to game code through small interfaces under
`src/platform`.

## Design rules

- Keep the implementation in C. Do not introduce C++.
- Prefer small modules with explicit responsibilities and narrow headers.
- Keep SDL and Win32 types out of game and engine headers.
- Preserve original game behaviour before considering enhancements.
- Replace DOS hardware access at subsystem boundaries rather than emulating
  VGA, Sound Blaster or DOS interrupts.
- Use ordinary flat address-space pointers and standard allocation on the
  native target; do not reproduce segmented-memory abstractions.
- Keep timing deterministic: simulation ticks must be independent of render
  presentation frequency.
- Retain original resource formats and data compatibility wherever practical.

## Planned subsystem boundaries

| Concern | Native implementation |
| --- | --- |
| Window and event lifecycle | SDL3 platform backend |
| Input | SDL3 input backend behind the input manager interface |
| Audio output | SDL3 audio backend behind the sound manager interface |
| Time | Monotonic SDL performance counter |
| Framebuffer presentation | SDL3 surface/texture presentation |
| Memory and page cache | Portable C implementation |
| Game simulation and resource formats | Existing engine code, incrementally ported |

## Initial build

The top-level CMake project detects an installed SDL3 CMake package. When SDL3
is available it builds the platform adapter as a static C library. The adapter
is intentionally not wired into the original DOS entry point yet; that
integration follows after the original compiler and memory assumptions have
been mapped.

On Windows, configure with a C compiler and an SDL3 development package:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

If SDL3 is installed outside the default CMake search paths, set
`SDL3_DIR` to the directory containing its package configuration.

## Next steps

1. Inventory compiler-specific syntax, segmented pointers and inline assembly.
2. Establish a portable type and memory layer.
3. Port the video manager to an indexed software framebuffer and SDL3
   presentation.
4. Port input, timing and audio managers independently.
5. Connect the native entry point to the existing game lifecycle.
6. Add regression checks for resource loading and deterministic game logic.
