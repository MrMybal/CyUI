cmake_minimum_required(VERSION 3.16)
project(CyUISelectiveConsumer LANGUAGES CXX)
get_filename_component(CYUI_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(CYUI_SELECTED_MODULE Core CACHE STRING "Single module under test")
set(CYUI_TEST_IMGUI_LIBRARY "" CACHE FILEPATH "Optional prebuilt host ImGui library")
if(NOT CYUI_TEST_IMGUI_LIBRARY STREQUAL "")
    add_library(host_imgui STATIC IMPORTED)
    set_target_properties(host_imgui PROPERTIES IMPORTED_LOCATION "${CYUI_TEST_IMGUI_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${CYUI_ROOT}/third_party/imgui")
else()
    add_library(host_imgui STATIC "${CYUI_ROOT}/third_party/imgui/imgui.cpp"
        "${CYUI_ROOT}/third_party/imgui/imgui_draw.cpp"
        "${CYUI_ROOT}/third_party/imgui/imgui_tables.cpp"
        "${CYUI_ROOT}/third_party/imgui/imgui_widgets.cpp")
    target_include_directories(host_imgui PUBLIC "${CYUI_ROOT}/third_party/imgui")
endif()
# The host pattern documented in README.md and docs/MODULES.md: plain (non-cache) variables,
# set before add_subdirectory. This project deliberately keeps cmake_minimum_required(3.16),
# so CyUI must honour them itself on the very first configure; if CyUI's cache defaults
# replaced them, the bundled cyui_imgui would appear (checked below) and modules.txt
# would not match the selection (checked by module_matrix.py).
set(CYUI_IMGUI_TARGET host_imgui)
set(CYUI_BUILD_DEMO OFF)
set(CYUI_BUILD_TESTS OFF)
set(CYUI_BUILD_AUDIO OFF)
set(CYUI_BUILD_GALLERY OFF)
# Audio and Gallery are selected through CYUI_MODULES like the other modules.
set(CYUI_MODULES "${CYUI_SELECTED_MODULE}")
add_subdirectory("${CYUI_ROOT}" cyui)
if(TARGET cyui_imgui OR TARGET cyui_demo)
    message(FATAL_ERROR "Selective consumer unexpectedly built a renderer or duplicate ImGui")
endif()
get_property(selected GLOBAL PROPERTY CYUI_ENABLED_MODULES)
file(WRITE "${CMAKE_BINARY_DIR}/modules.txt" "${selected}")
if(CYUI_SELECTED_MODULE STREQUAL "Audio" OR CYUI_SELECTED_MODULE STREQUAL "Gallery")
    # These two have no checks in features.cpp: run their own smoke checks instead.
    string(TOUPPER "${CYUI_SELECTED_MODULE}" upper)
    add_executable(consumer "${CYUI_ROOT}/tests/module_smoke.cpp")
    target_compile_definitions(consumer PRIVATE CYUI_TEST_${upper}=1)
else()
    add_executable(consumer "${CYUI_ROOT}/tests/features.cpp")
endif()
target_link_libraries(consumer PRIVATE CyUI::${CYUI_SELECTED_MODULE})
if(MSVC)
    target_compile_options(consumer PRIVATE /utf-8 /EHsc)
endif()
