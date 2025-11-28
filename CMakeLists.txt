cmake_minimum_required(VERSION 3.13)
project(cnes LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.2.x
)
FetchContent_MakeAvailable(SDL3)

file(GLOB MAPPERS "${CMAKE_CURRENT_SOURCE_DIR}/src/mapper/*.c")

add_executable(cnes
    src/apu.c
    src/cart.c
    src/cpu.c
    src/gui.c
    src/input.c
    src/main.c
    src/nes.c
    src/ppu.c
    ${MAPPERS}
)

target_include_directories(cnes PRIVATE
    external/
    external/imgui
)

set_property(TARGET cnes PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
set(CMAKE_OPTIMIZE_DEPENDENCIES_RELEASE TRUE)

if (MSVC)
    target_compile_options(cnes PRIVATE
        /W4
        $<$<CONFIG:Debug>:/Od>
        $<$<CONFIG:Debug>:/Zi>
        $<$<CONFIG:Debug>:/MDd>
        $<$<CONFIG:Release>:/O2>
        $<$<CONFIG:Release>:/MD>
    )
else()
    target_compile_options(cnes PRIVATE
        -Wall
        $<$<CONFIG:Debug>:-O2>
        $<$<CONFIG:Debug>:-g>
        $<$<CONFIG:Debug>:-fsanitize=address>
        $<$<CONFIG:Debug>:-fno-omit-frame-pointer>
        $<$<CONFIG:Debug>:-fno-optimize-sibling-calls>
        $<$<CONFIG:Debug>:-DNES_PROFILING=1>
        $<$<CONFIG:Debug>:-pg>
        $<$<CONFIG:Release>:-O3>
        $<$<CONFIG:Release>:-DNDEBUG>
        $<$<CONFIG:Release>:-march=native>
        $<$<CONFIG:Release>:-mtune=native>
        $<$<CONFIG:Release>:-fstrict-aliasing>
        $<$<CONFIG:Release>:-ffast-math>
    )
endif()

target_link_options(cnes PRIVATE
    $<$<CONFIG:Debug>:-fsanitize=address>
    $<$<CONFIG:Debug>:-pg>
    $<$<CONFIG:Release>:-flto>
)

file(GLOB IMGUI_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/backends/*.cpp
)

add_library(imgui STATIC ${IMGUI_SRC})
target_include_directories(imgui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/external
    ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui
)
target_link_libraries(imgui PUBLIC SDL3::SDL3)

set_property(TARGET cnes PROPERTY LINKER_LANGUAGE CXX)
target_link_libraries(cnes PRIVATE SDL3::SDL3 imgui m)
