# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

ccompose is a small C11 wrapper around [Clay](https://github.com/nicbarker/clay) that gives Clay's immediate-mode layout engine a Jetpack-Compose-shaped, declarative block syntax. It ships with an optional bundled raylib renderer backend. Vendored dependencies: `external/clay/` and `include/clay.h` (Clay), `renderer/raylib/` (Clay raylib renderer glue).

> Status: pre-release (v0.2.0). APIs and build layout can change.

## Build / test / run

```bash
# Configure + build (raylib backend ON by default; auto-fetches raylib 5.5 if find_package fails)
cmake -S . -B build
cmake --build build

# Tests (CTest + assert)
ctest --test-dir build --output-on-failure

# Run a single test
./build/ccompose_smoke_test

# Run the demo
./build/examples/ccompose_demo               # Linux/macOS
./build/examples/Debug/ccompose_demo.exe     # Windows (multi-config)

# Headless / CI build (no raylib, no demo target)
cmake -S . -B build -DCCOMPOSE_BACKEND_RAYLIB=OFF
```

Useful CMake options:
- `-DCCOMPOSE_BACKEND_RAYLIB=OFF` — skip raylib backend; defines `CCOMPOSE_NO_BACKEND` for consumers.
- `-DCCOMPOSE_BUILD_TESTS=OFF`, `-DCCOMPOSE_BUILD_EXAMPLES=OFF` — optional targets (default ON when top-level).
- `-DCCOMPOSE_C_STANDARD=11|17|23` — language standard (default 11).
- `-DCCOMPOSE_RAYLIB_VERSION=5.5` — override fetched raylib tag.

Windows: easiest path is `vcpkg install raylib:x64-windows` then configure with `-DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake`. Otherwise CMake fetches raylib from source.

Linux (when fetching raylib from source): install X11/GL dev headers — `libgl1-mesa-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev` (Debian) or the Fedora equivalents.

## Architecture

Three layers, all statically linked into one `ccompose` static library target:

1. **Clay** (`include/clay.h`, vendored) — the actual layout engine. Holds all per-frame state in its own arena; ccompose adds no hidden tree or runtime allocation per frame.
2. **ccompose core** (`src/ccompose.c`, `include/ccompose.h`) — thin macro + function layer that wraps Clay in a scoped-block DSL (`Column("id", .field = value, ...) { children }`, `Row`, `Box`, `Element`, `Text`, `TextN`). Each container macro expands to a single-iteration `for` loop that opens a Clay element on entry and closes it on exit. Every `.field` maps 1:1 onto Clay's own config structs — there is no new mental model on top of Clay. Because containers are loops, `break` inside a block will skip the close step; use `return`/`goto` carefully.
3. **raylib backend** (`renderer/raylib/`) — window + renderer glue. Included in the public include path so that `#include "ccompose.h"` also pulls in `raylib.h` and lets user code mix raylib types (`Vector2`, `Font`, `FLAG_*`) with ccompose macros. Compiled out when `CCOMPOSE_NO_BACKEND` is defined (`-DCCOMPOSE_BACKEND_RAYLIB=OFF`).

Transitions (v0.2.0+): `DefineTransition(name, duration, .enter = {...}, .exit = {...})` declares a reusable preset at file scope. Built-in easings: `CC_EaseOut` (default), `CC_Linear`, `CC_EaseIn`, `CC_EaseInOut`. Transition state travels via `CC_TransitionEffect`; see `include/ccompose.h` for the full field list.

Public API uses `CC_*` prefix (types, lifecycle functions, macros). File-local helpers use `cc__*` static symbols. Lifecycle: `CC_SetWindow` → `CC_Init` → loop(`CC_Begin` / container blocks / `CC_End`) while `CC_Running()` → `CC_Shutdown`. Other entry points: `CC_SetWindowFlags`, `CC_SetBackground`, `CC_SetErrorHandler`, `CC_LoadFont` (max 16 fonts via `CC_MAX_FONTS`), `CC_SetViewport`. Container macros: `Column`, `Row`, `Box`, `Element`, `Button`. Text macros: `Text`, `TextN`. Leaf helpers: `HSpacer`, `VSpacer`, `HDivider`, `VDivider` (auto-id, safe in loops). Interaction: `CC_Hovered("id")`, `CC_Clicked("id")` — query before or after the `Button` block; the id must be a string literal matching the button's. Sugar helpers: `Fit()`, `Grow()`, `Fixed()`, `Percent()`, `PadAll()`, `Pad()`, `RadiusAll()`, `Color()`, `AlignStart/Center/End`.

### Headless build split

For tests/CI there is a second internal static library target `ccompose_headless` that recompiles `src/ccompose.c` with `-DCCOMPOSE_NO_BACKEND`. The smoke test links against this so it can run without a display even in builds where the raylib backend is otherwise enabled. When adding tests, link them to `ccompose_headless`, not `ccompose`.

### Vendored code

`external/clay/` and `renderer/raylib/` are treated as vendored — Clay and the raylib renderer both emit their own warnings, so `ccompose`'s TU is built with `-w` to keep third-party noise out. Don't locally edit vendored sources unless you are intentionally updating the vendor drop, and document version/source rationale in the commit/PR when you do.

## Conventions

- C11+; 4-space indent; no tabs.
- Public symbols `CC_*`, internal statics `cc__*`, files lowercase `snake_case`.
- Prefer reusing existing Clay/ccompose helpers before introducing new abstractions.
- Tests live in `tests/` as `<feature>_test.c` / `<scope>_smoke_test.c`, registered via `add_test(...)` in `CMakeLists.txt`, must stay deterministic and headless (use the `CCOMPOSE_NO_BACKEND` path).

## Commit protocol (from AGENTS.md)

This repo follows the "Lore" commit protocol:
- Subject explains **why**, not what.
- Include trailers when relevant: `Constraint:`, `Rejected:`, `Confidence:`, `Scope-risk:`, `Directive:`, `Tested:`, `Not-tested:`.
- Keep commits small and reversible; don't mix refactors with behavior changes.
- PRs need a problem/solution summary, build + `ctest` evidence, linked issues, and screenshots/GIFs for UI-visible example changes.

## Further reading

- `include/ccompose.h` — authoritative API reference; the file header walks through the mental model, lifecycle, and every macro.
- `examples/demo.c` — end-to-end raylib-backed example.
