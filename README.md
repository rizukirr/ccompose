# ccompose

Small C wrapper around [Clay](https://github.com/nicbarker/clay) for building immediate-mode UI layouts, with optional raylib rendering support and Jetpack Compose like syntax.

## Demo Video

<video src="assets/demo.mp4" controls width="960"></video>

[Download demo video](assets/demo.mp4)

## Requirements

- CMake 3.16+
- C compiler with C11 support

## Build

```bash
cmake -S . -B build
cmake --build build
```

## Test

```bash
ctest --test-dir build --output-on-failure
```

## Run Demo

```bash
./build/examples/ccompose_demo
```

## Headless Build (No raylib)

```bash
cmake -S . -B build -DCCOMPOSE_BACKEND_RAYLIB=OFF
cmake --build build
```
