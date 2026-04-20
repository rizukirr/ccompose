cmake_minimum_required(VERSION 3.23)

project(ccompose
    VERSION 0.2.0
    DESCRIPTION "Jetpack-Compose-shaped wrapper around Clay"
    HOMEPAGE_URL "https://github.com/rizukirr/ccompose"
    LANGUAGES C
)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(CMakeDependentOption)

# ---------- Options ----------

option(CCOMPOSE_BACKEND_RAYLIB
    "Build ccompose with the bundled raylib backend (window + renderer)" ON)
option(CCOMPOSE_BACKEND_TERMBOX2
    "Build ccompose with the termbox2 TUI backend (mutually exclusive with raylib)" OFF)
if(CCOMPOSE_BACKEND_TERMBOX2)
    # Termbox2 wins; quietly disable raylib so the default ON doesn't trip the conflict.
    set(CCOMPOSE_BACKEND_RAYLIB OFF CACHE BOOL "" FORCE)
endif()

set(CCOMPOSE_TERMBOX2_REVISION "master" CACHE STRING
    "Git revision of termbox2 to fetch (branch, tag, or commit)")

option(CCOMPOSE_BUILD_EXAMPLES
    "Build example programs" ${PROJECT_IS_TOP_LEVEL})
option(CCOMPOSE_BUILD_TESTS
    "Build ccompose tests" ${PROJECT_IS_TOP_LEVEL})
option(CCOMPOSE_WARNINGS_AS_ERRORS
    "Treat ccompose's own warnings as errors (vendored code stays silent)" OFF)

# Install only makes sense for native builds where raylib was located via
# find_package. Under Emscripten raylib is FetchContent'd into the build tree
# and isn't in an export set, so install(EXPORT) would fail.
cmake_dependent_option(CCOMPOSE_INSTALL
    "Generate install targets and package config" ON
    "PROJECT_IS_TOP_LEVEL;NOT EMSCRIPTEN" OFF)

# Emscripten auto-detected via CMAKE_TOOLCHAIN_FILE=<emsdk>/.../Emscripten.cmake.
# When building for web we swap raylib's platform and use emscripten_set_main_loop
# in the CC_RUN_LOOP macro (see include/ccompose.h).
cmake_dependent_option(CCOMPOSE_TARGET_WEB
    "Build ccompose for web (WASM) via Emscripten" ON
    "EMSCRIPTEN" OFF)

set(CCOMPOSE_C_STANDARD "11" CACHE STRING
    "C standard used to build ccompose targets (11, 17, 23)")
set_property(CACHE CCOMPOSE_C_STANDARD PROPERTY STRINGS 11 17 23)

set(CCOMPOSE_RAYLIB_VERSION "5.5" CACHE STRING
    "raylib version to fetch when find_package(raylib) fails")

if(PROJECT_IS_TOP_LEVEL)
    set(CMAKE_C_STANDARD ${CCOMPOSE_C_STANDARD})
    set(CMAKE_C_STANDARD_REQUIRED ON)
    # CMAKE_C_EXTENSIONS is scoped per-target in ccompose_define_library so
    # that FetchContent'd dependencies (raylib + miniaudio) keep their GNU
    # extensions — required for EM_ASM under Emscripten.
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

    if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
        set(CMAKE_BUILD_TYPE "Release" CACHE STRING
            "Build type (Debug, Release, RelWithDebInfo, MinSizeRel)" FORCE)
        set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
            Debug Release RelWithDebInfo MinSizeRel)
    endif()

    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
endif()

# ---------- Helpers ----------

function(ccompose_apply_compile_flags target)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        target_compile_options(${target} PRIVATE -w)
    elseif(CMAKE_C_COMPILER_ID STREQUAL "MSVC")
        target_compile_options(${target} PRIVATE /w)
    endif()
    if(CCOMPOSE_WARNINGS_AS_ERRORS AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        target_compile_options(${target} PRIVATE -Werror)
    elseif(CCOMPOSE_WARNINGS_AS_ERRORS AND CMAKE_C_COMPILER_ID STREQUAL "MSVC")
        target_compile_options(${target} PRIVATE /WX)
    endif()
endfunction()

function(ccompose_define_library target)
    cmake_parse_arguments(arg "HEADLESS" "" "" ${ARGN})

    add_library(${target} STATIC src/ccompose.c)
    target_compile_features(${target} PUBLIC c_std_${CCOMPOSE_C_STANDARD})
    set_target_properties(${target} PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        C_EXTENSIONS OFF
        C_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
    )

    target_include_directories(${target} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    )

    if(arg_HEADLESS)
        target_compile_definitions(${target} PUBLIC CCOMPOSE_NO_BACKEND)
    endif()

    # Backend-selection macros are propagated to consumers so the public
    # header can gate raylib- or termbox2-specific declarations.
    if(NOT arg_HEADLESS AND CCOMPOSE_BACKEND_RAYLIB)
        target_compile_definitions(${target} PUBLIC CCOMPOSE_BACKEND_RAYLIB)
    endif()
    if(NOT arg_HEADLESS AND CCOMPOSE_BACKEND_TERMBOX2)
        target_compile_definitions(${target} PUBLIC CCOMPOSE_BACKEND_TERMBOX2)
    endif()

    if(NOT WIN32 AND NOT EMSCRIPTEN)
        target_link_libraries(${target} PUBLIC m)
    endif()

    ccompose_apply_compile_flags(${target})
endfunction()

# ---------- raylib dependency ----------

function(ccompose_link_raylib target)
    if(CCOMPOSE_TARGET_WEB)
        set(PLATFORM "Web" CACHE STRING "" FORCE)
    endif()

    find_package(raylib ${CCOMPOSE_RAYLIB_VERSION} CONFIG QUIET)
    if(NOT raylib_FOUND)
        message(STATUS
            "raylib not found via find_package; fetching ${CCOMPOSE_RAYLIB_VERSION} from GitHub")
        include(FetchContent)
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

    target_include_directories(${target} PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/renderer/raylib>
    )

    target_link_libraries(${target} PUBLIC raylib)
endfunction()

function(ccompose_apply_web_flags target)
    if(NOT CCOMPOSE_TARGET_WEB)
        return()
    endif()
    target_link_options(${target} PUBLIC
        "-sUSE_GLFW=3"
        "-sASYNCIFY"
        "-sALLOW_MEMORY_GROWTH=1"
        "-sTOTAL_MEMORY=67108864"
        "-sFORCE_FILESYSTEM=1"
        "--shell-file=${CMAKE_SOURCE_DIR}/examples/shell.html"
        "--preload-file=${CMAKE_SOURCE_DIR}/examples/resources@examples/resources"
    )
    set_target_properties(${target} PROPERTIES SUFFIX ".html")
endfunction()

# ---------- termbox2 dependency ----------

function(ccompose_link_termbox2 target)
    if(EMSCRIPTEN)
        message(FATAL_ERROR "termbox2 backend is incompatible with Emscripten (needs termios/pty)")
    endif()
    include(FetchContent)
    FetchContent_Declare(
        termbox2
        GIT_REPOSITORY https://github.com/termbox/termbox2.git
        GIT_TAG        ${CCOMPOSE_TERMBOX2_REVISION}
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(termbox2)
    # termbox2 ships as a single header (termbox2.h) at the repo root.
    target_include_directories(${target} PRIVATE
        $<BUILD_INTERFACE:${termbox2_SOURCE_DIR}>
    )
    target_include_directories(${target} PRIVATE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/renderer/termbox2>
    )
    # termbox2.h pulls in errno / fileno / ioctl / clock_gettime and
    # assumes a GNU-flavored Linux toolchain. The helper turned off
    # C_EXTENSIONS for ccompose, which puts gcc in strict ISO C11 mode and
    # breaks errno's macro expansion. Re-enable extensions on this target.
    set_target_properties(${target} PROPERTIES C_EXTENSIONS ON)
    target_compile_definitions(${target} PRIVATE
        _GNU_SOURCE=1
    )
    # Enable termbox2's 32-bit attribute mode so TB_OUTPUT_TRUECOLOR is
    # available (24-bit RGB packed colors). Without this, only the 16-color
    # / 256-color palettes are exposed.
    target_compile_definitions(${target} PRIVATE TB_OPT_ATTR_W=32)
endfunction()

# ---------- ccompose library target ----------

ccompose_define_library(ccompose)
add_library(ccompose::ccompose ALIAS ccompose)

if(CCOMPOSE_BACKEND_RAYLIB)
    ccompose_link_raylib(ccompose)
elseif(CCOMPOSE_BACKEND_TERMBOX2)
    ccompose_link_termbox2(ccompose)
else()
    target_compile_definitions(ccompose PUBLIC CCOMPOSE_NO_BACKEND)
endif()

# ---------- Examples ----------

if(CCOMPOSE_BUILD_EXAMPLES AND (CCOMPOSE_BACKEND_RAYLIB OR CCOMPOSE_BACKEND_TERMBOX2))
    add_subdirectory(examples)
endif()

# ---------- Tests ----------

if(CCOMPOSE_BUILD_TESTS)
    enable_testing()

    ccompose_define_library(ccompose_headless HEADLESS)

    function(ccompose_add_test name source)
        add_executable(${name} ${source})
        target_link_libraries(${name} PRIVATE ccompose_headless)
        add_test(NAME ${name} COMMAND ${name})
    endfunction()

    ccompose_add_test(ccompose_smoke_test         tests/smoke_test.c)
    ccompose_add_test(ccompose_transition_test    tests/transition_test.c)
    ccompose_add_test(compose_spacer_divider_test tests/spacer_divider_test.c)
endif()

# ---------- Install / export ----------

if(CCOMPOSE_INSTALL)
    install(TARGETS ccompose
        EXPORT ccomposeTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    install(DIRECTORY include/
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.h"
    )

    if(CCOMPOSE_BACKEND_RAYLIB)
        install(DIRECTORY renderer/raylib/
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
            FILES_MATCHING PATTERN "*.h"
        )
    endif()

    install(EXPORT ccomposeTargets
        FILE ccomposeTargets.cmake
        NAMESPACE ccompose::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ccompose
    )

    set(CCOMPOSE_CONFIG_IN "${CMAKE_CURRENT_BINARY_DIR}/ccomposeConfig.cmake.in")
    file(WRITE  "${CCOMPOSE_CONFIG_IN}" "@PACKAGE_INIT@\n")
    file(APPEND "${CCOMPOSE_CONFIG_IN}" "include(CMakeFindDependencyMacro)\n")
    file(APPEND "${CCOMPOSE_CONFIG_IN}"
        "if(@CCOMPOSE_BACKEND_RAYLIB@)\n"
        "    find_dependency(raylib @CCOMPOSE_RAYLIB_VERSION@)\n"
        "endif()\n")
    file(APPEND "${CCOMPOSE_CONFIG_IN}"
        "include(\"\${CMAKE_CURRENT_LIST_DIR}/ccomposeTargets.cmake\")\n")

    configure_package_config_file(
        "${CCOMPOSE_CONFIG_IN}"
        "${CMAKE_CURRENT_BINARY_DIR}/ccomposeConfig.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ccompose
    )
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/ccomposeConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )
    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/ccomposeConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/ccomposeConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ccompose
    )
endif()

# ---------- Summary ----------

if(PROJECT_IS_TOP_LEVEL)
    message(STATUS "")
    message(STATUS "ccompose ${PROJECT_VERSION}")
    message(STATUS "  C standard          : c${CCOMPOSE_C_STANDARD}")
    message(STATUS "  Build type          : ${CMAKE_BUILD_TYPE}")
    message(STATUS "  raylib backend      : ${CCOMPOSE_BACKEND_RAYLIB}")
    message(STATUS "  termbox2 backend    : ${CCOMPOSE_BACKEND_TERMBOX2}")
    message(STATUS "  Web (Emscripten)    : ${CCOMPOSE_TARGET_WEB}")
    message(STATUS "  Examples            : ${CCOMPOSE_BUILD_EXAMPLES}")
    message(STATUS "  Tests               : ${CCOMPOSE_BUILD_TESTS}")
    message(STATUS "  Install             : ${CCOMPOSE_INSTALL}")
    message(STATUS "")
endif()
