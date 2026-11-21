cmake_minimum_required(VERSION 3.20)
project(pacdll VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_POLICY_DEFAULT_CMP0091 NEW)

# libraries
include_directories(${CMAKE_SOURCE_DIR}/src)
include_directories(SYSTEM ${CMAKE_SOURCE_DIR}/third_party)

# imgui
add_library(imgui STATIC
    ${CMAKE_SOURCE_DIR}/third_party/imgui/imgui.cpp
    ${CMAKE_SOURCE_DIR}/third_party/imgui/imgui_demo.cpp
    ${CMAKE_SOURCE_DIR}/third_party/imgui/imgui_draw.cpp
    ${CMAKE_SOURCE_DIR}/third_party/imgui/imgui_tables.cpp
    ${CMAKE_SOURCE_DIR}/third_party/imgui/imgui_widgets.cpp
    ${CMAKE_SOURCE_DIR}/third_party/imgui/backends/imgui_impl_dx11.cpp
    ${CMAKE_SOURCE_DIR}/third_party/imgui/backends/imgui_impl_win32.cpp
)

target_include_directories(imgui PUBLIC
    ${CMAKE_SOURCE_DIR}/third_party/imgui
    ${CMAKE_SOURCE_DIR}/third_party/imgui/backends
)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(SRC_FILE_LIST
    ${CMAKE_CURRENT_LIST_DIR}/src/pacdll.cpp
)

add_library(${PROJECT_NAME} SHARED ${SRC_FILE_LIST})

# compile flags
if (MSVC)
    # cl.exe and clang-cl
    target_compile_options(${PROJECT_NAME} PRIVATE /W3)

    set(SUBSYSTEM_LINKER_OPTIONS 
        /SUBSYSTEM:WINDOWS
    )
else()
    # clang++ and g++
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic)

    set(SUBSYSTEM_LINKER_OPTIONS
        -Wl,/subsystem:windows
    )
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES COMPILE_WARNING_AS_ERROR ON)
target_compile_definitions(${PROJECT_NAME} PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    UNICODE
    _UNICODE
    PACDLL_EXPORTS
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    shell32
    user32
    gdi32
    imgui
    d3d11
    dxguid
    d3dcompiler
    dwmapi
)

# add_executable(test WIN32 ${CMAKE_CURRENT_SOURCE_DIR}/src/test.cpp)
