# Example programs for ccompose. Built when CCOMPOSE_BUILD_EXAMPLES is ON
# together with one of the rendering backends — the parent CMakeLists.txt
# enforces that via its add_subdirectory() guard.
#
# Demos split by backend:
#   - raylib   : demo.c, landing.c   (raylib-typed images/fonts)
#   - termbox2 : tui_demo.c          (text-only, monospace cell grid)

function(ccompose_add_example target source)
    add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE ccompose)
    set_target_properties(${target} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples
    )
    ccompose_apply_web_flags(${target})
endfunction()

if(CCOMPOSE_BACKEND_RAYLIB)
    ccompose_add_example(ccompose_demo    demo.c)
    ccompose_add_example(ccompose_landing landing.c)
elseif(CCOMPOSE_BACKEND_TERMBOX2)
    ccompose_add_example(ccompose_tui_demo tui_demo.c)
endif()
