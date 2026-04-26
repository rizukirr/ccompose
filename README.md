# ccompose

Small C wrapper around [Clay](https://github.com/nicbarker/clay) for building immediate-mode UI layouts, with a raylib renderer and Jetpack Compose like syntax.

> [!WARNING]
> **Not production ready.** ccompose is under heavy development. APIs, build layout, and behavior can change without notice. Expect breakage, missing features, and rough edges. Do not use this in anything you care about yet.

## Demo Video

### Desktop
https://github.com/user-attachments/assets/80fa3cc7-f1c5-4ab4-b352-da707138f55c

### Web Asm
https://github.com/user-attachments/assets/38088b72-dd5f-4c1e-8d39-860e27716d5a

## Example

```c
#include "ccompose.h"

int main(void) {
    CC_SetWindow(480, 320, "hi");
    CC_Init();

    bool quit = false;
    while (CC_Running() && !quit) {
        CC_Begin();

        Column("Root",
               .layout = { .sizing = { Grow(), Grow() },
                           .padding = PadAll(24),
                           .childGap = 12,
                           .childAlignment = { .x = AlignCenter(),
                                               .y = AlignMiddle() } },
               .backgroundColor = Color(18, 18, 20, 255)) {

            Text("Hello, ccompose!",
                 .textColor = Color(236, 236, 236, 255),
                 .fontSize  = 28);

            if (CC_Clicked("Quit")) quit = true;
            Button("Quit",
                   .layout = { .padding = PadAll(12) },
                   .backgroundColor = CC_Hovered("Quit")
                                          ? Color(80, 140, 220, 255)
                                          : Color(40, 100, 200, 255),
                   .cornerRadius = RadiusAll(6)) {
                Text("Quit",
                     .textColor = Color(255, 255, 255, 255),
                     .fontSize  = 16);
            }
        }

        CC_End();
    }

    CC_Shutdown();
    return 0;
}
```

See the [wiki](https://github.com/rizukirr/ccompose/wiki) for full API docs,
more examples, and the Clay field reference.

## Backends

ccompose ships a raylib rendering backend. Or disable rendering entirely for tests/CI:

| Backend    | CMake flag                               | Renders to           | Dependencies               |
| ---------- | ---------------------------------------- | -------------------- | -------------------------- |
| raylib     | `-DCCOMPOSE_BACKEND_RAYLIB=ON` (default) | native window / WASM | raylib 5.5 (auto-fetched) |
| *headless* | `-DCCOMPOSE_BACKEND_RAYLIB=OFF`          | none (test/CI)       | none                       |

Image and raylib-typed APIs (`CC_LoadImage`, `Image()`, raylib `Texture2D` / `Font`) are only declared when the raylib backend is on.

## Requirements

- CMake 3.23+
- C compiler with C11 support
- (Optional) raylib 5.5 - auto-fetched from source if not already installed

## Build

### Linux / macOS

```bash
cmake -S . -B build
cmake --build build
```

If raylib is already installed (e.g. `apt install libraylib-dev`, `brew install raylib`), CMake will find and use it. Otherwise it falls back to downloading and building raylib 5.5 from source as part of the build.

When fetching raylib from source on Linux, install the X11/GL dev headers first:

```bash
# Debian/Ubuntu
sudo apt install libgl1-mesa-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev

# Fedora
sudo dnf install mesa-libGL-devel libX11-devel libXrandr-devel libXinerama-devel libXcursor-devel libXi-devel
```

### Windows

The simplest path is raylib via [vcpkg](https://vcpkg.io):

```bash
vcpkg install raylib:x64-windows

cmake -S . -B build ^
  -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake ^
  -DVCPKG_TARGET_TRIPLET=x64-windows
cmake --build build --config Debug
```

If you skip the vcpkg step, CMake will fall back to downloading raylib 5.5 from source automatically - no extra system packages needed on Windows (MSVC alone is enough).

You can override the fetched raylib version with `-DCCOMPOSE_RAYLIB_VERSION=5.0` if needed.

## Test

```bash
ctest --test-dir build --output-on-failure
```

## Run Demo

```bash
# Linux / macOS
./build/examples/ccompose_demo

# Windows
./build/examples/Debug/ccompose_demo.exe
```

## Headless Build (No renderer)

Skips the renderer and the demo target - useful for CI or environments without a display:

```bash
cmake -S . -B build -DCCOMPOSE_BACKEND_RAYLIB=OFF
cmake --build build
```

## Quit handling & input

`CC_Running()` returns false on the window close button and on `ESC` by default. Customize with `CC_SetQuitHandler` (receives `void *user`, returns `true` to stop the loop) or call `CC_RequestQuit()` from anywhere - e.g. a `Button` click handler.

```c
static bool quit_on_q_or_esc(void *user) {
    (void)user;
    return CC_KeyPressed(CC_KEY_Q) || CC_KeyPressed(CC_KEY_ESCAPE);
}

CC_SetQuitHandler(quit_on_q_or_esc, NULL);
```

`CC_KeyPressed(CC_KEY_*)` is an edge-triggered keyboard check. The enum covers `ESCAPE`, `ENTER`, `SPACE`, `TAB`, `Q/W/E/R`, and arrow keys - add more as needed. Under raylib it maps to `IsKeyPressed`. Setting a custom handler replaces the default ESC binding, so re-add `CC_KEY_ESCAPE` inside your handler if you want to keep it.

## Web Build (WASM via Emscripten)

The same C source builds for the browser. Install the [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html) and activate it, then configure through `emcmake`:

```bash
# Activate emsdk in the current shell
source /path/to/emsdk/emsdk_env.sh

emcmake cmake -S . -B build-web
cmake --build build-web
```

This produces `.html`, `.js`, and `.wasm` per example under `build-web/examples/`. Serve them over HTTP (browsers block `file://` for WASM):

```bash
python -m http.server -d build-web/examples 8080
# open http://localhost:8080/ccompose_demo.html
```

`CCOMPOSE_TARGET_WEB` auto-enables under the emsdk toolchain. Writing portable apps requires one rule: replace your `while (CC_Running()) { ... }` loop with `CC_RUN_LOOP(frame)`, where `frame` is a `void (*)(void)` that calls `CC_Begin()` / builds the tree / `CC_End()`. Natively it expands to a plain while-loop; under `__EMSCRIPTEN__` it hands off to `emscripten_set_main_loop` because the browser owns the event loop.

```c
static void frame(void) {
    CC_Begin();
    Column("Root", ...) { /* ... */ }
    CC_End();
}

int main(void) {
    CC_SetWindow(800, 600, "ccompose");
    CC_Init();
    CC_RUN_LOOP(frame);   // native + web
    CC_Shutdown();
}
```

Put any state the frame reads at file scope (the callback takes no args). See `examples/landing.c` for a full portable example.

Tune link flags in `CMakeLists.txt`'s `ccompose_apply_web_flags` (memory size, ASYNCIFY, preloaded resources, `--shell-file`).

## Support

[![ko-fi](https://ko-fi.com/img/githubbutton_sm.svg)](https://ko-fi.com/rizukirr)
