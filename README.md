# ccompose

Small C wrapper around [Clay](https://github.com/nicbarker/clay) for building immediate-mode UI layouts, with optional raylib rendering support and Jetpack Compose like syntax.

## Demo Video

https://github.com/user-attachments/assets/d81e628b-dc51-41b5-b5ed-47d669926afa


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
