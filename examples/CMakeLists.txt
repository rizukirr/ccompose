# Example programs for ccompose. Built when both
# CCOMPOSE_BUILD_EXAMPLES and CCOMPOSE_BACKEND_RAYLIB are ON — the parent
# CMakeLists.txt enforces that via its add_subdirectory() guard.
#
# Each executable lives in examples/<name>.c, links against the
# ccompose target, and is written to build/examples/<name> so users can
# run them with a predictable path.

function(ccompose_add_example target source)
    add_executable(${target} ${source})
    target_link_libraries(${target} PRIVATE ccompose)
    set_target_properties(${target} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/examples
    )
endfunction()

ccompose_add_example(ccompose_demo   demo.c)
ccompose_add_example(muslimtify_gui  muslimtify_gui.c)
