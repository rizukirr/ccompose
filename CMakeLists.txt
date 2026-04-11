cmake_minimum_required(VERSION 3.23)
project(ccompose
    VERSION 0.2.0
    DESCRIPTION "Jetpack-Compose-shaped wrapper around Clay"
    LANGUAGES C
)

# ---------- Options ----------

option(CCOMPOSE_BACKEND_RAYLIB
    "Build ccompose with the bundled raylib backend (window + renderer)" ON)
option(CCOMPOSE_BUILD_EXAMPLES
    "Build example programs" ${PROJECT_IS_TOP_LEVEL})
option(CCOMPOSE_BUILD_TESTS
    "Build ccompose tests" ${PROJECT_IS_TOP_LEVEL})

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CCOMPOSE_C_STANDARD "11" CACHE STRING
    "C standard used to build ccompose targets (11, 17, 23)")
set_property(CACHE CCOMPOSE_C_STANDARD PROPERTY STRINGS 11 17 23)

if(PROJECT_IS_TOP_LEVEL)
    set(CMAKE_C_STANDARD ${CCOMPOSE_C_STANDARD})
    set(CMAKE_C_STANDARD_REQUIRED ON)
    set(CMAKE_C_EXTENSIONS OFF)
endif()

# ---------- ccompose library ----------

add_library(ccompose STATIC src/ccompose.c)
add_library(ccompose::ccompose ALIAS ccompose)

target_compile_features(ccompose PUBLIC c_std_${CCOMPOSE_C_STANDARD})

target_include_directories(ccompose PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Clay and the bundled raylib renderer each emit their own warnings. Keep
# the library's own TU quiet — downstream targets can enable their own
# warning flags without being flooded by third-party noise.
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(ccompose PRIVATE -w)
endif()

if(CCOMPOSE_BACKEND_RAYLIB)
    # Try to locate raylib via its CMake package config (provided by vcpkg,
    # Homebrew, distro packages, or a source build's install tree). If it
    # isn't found, fall back to fetching and building it from source so the
    # project builds out of the box on fresh machines.
    set(CCOMPOSE_RAYLIB_VERSION "5.5" CACHE STRING
        "raylib version to fetch when find_package(raylib) fails")

    find_package(raylib ${CCOMPOSE_RAYLIB_VERSION} CONFIG QUIET)
    if(NOT raylib_FOUND)
        message(STATUS
            "raylib not found via find_package; fetching ${CCOMPOSE_RAYLIB_VERSION} from GitHub")
        include(FetchContent)
        # Keep raylib's own examples/games out of our build.
        set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(BUILD_GAMES    OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(
            raylib
            GIT_REPOSITORY https://github.com/raysan5/raylib.git
            GIT_TAG        ${CCOMPOSE_RAYLIB_VERSION}
            GIT_SHALLOW    TRUE
        )
        FetchContent_MakeAvailable(raylib)
    endif()

    # The bundled raylib renderer lives at renderer/raylib/. We expose that
    # directory PUBLICly so consumers that #include "ccompose.h" pick up
    # the vendored raylib.h too (ccompose.h #include "raylib.h").
    target_include_directories(ccompose PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/renderer/raylib>
    )

    # Link raylib + libm (Clay uses math functions; libm is implicit on Windows).
    if(NOT WIN32)
        target_link_libraries(ccompose PUBLIC raylib m)
    else()
        target_link_libraries(ccompose PUBLIC raylib)
    endif()
else()
    target_compile_definitions(ccompose PUBLIC CCOMPOSE_NO_BACKEND)
    if(NOT WIN32)
        target_link_libraries(ccompose PUBLIC m)
    endif()
endif()

# ---------- Examples ----------
#
# Each example lives in examples/<name>.c and is registered in
# examples/CMakeLists.txt. Keeping the list there means adding a new
# example is a one-line change in the examples dir and doesn't touch
# the top-level build file.

if(CCOMPOSE_BUILD_EXAMPLES AND CCOMPOSE_BACKEND_RAYLIB)
    add_subdirectory(examples)
endif()

# ---------- Tests ----------

if(CCOMPOSE_BUILD_TESTS)
    enable_testing()

    # The headless smoke test uses CCOMPOSE_NO_BACKEND so it can run on
    # CI without a display. It builds its own tiny ccompose variant by
    # compiling src/ccompose.c with -DCCOMPOSE_NO_BACKEND.
    add_library(ccompose_headless STATIC src/ccompose.c)
    target_compile_features(ccompose_headless PUBLIC c_std_${CCOMPOSE_C_STANDARD})
    target_include_directories(ccompose_headless PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    )
    target_compile_definitions(ccompose_headless PUBLIC CCOMPOSE_NO_BACKEND)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        target_compile_options(ccompose_headless PRIVATE -w)
    endif()
    if(NOT WIN32)
        target_link_libraries(ccompose_headless PUBLIC m)
    endif()

    add_executable(ccompose_smoke_test tests/smoke_test.c)
    target_link_libraries(ccompose_smoke_test PRIVATE ccompose_headless)
    add_test(NAME smoke COMMAND ccompose_smoke_test)
endif()
