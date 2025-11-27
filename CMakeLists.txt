cmake_minimum_required(VERSION 3.13)
project(cnes LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

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

set_property(TARGET cnes PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
set(CMAKE_OPTIMIZE_DEPENDENCIES_RELEASE TRUE)

target_compile_options(cnes PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    $<$<CONFIG:Debug>:-O2>
    $<$<CONFIG:Debug>:-g>
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

target_link_options(cnes PRIVATE
    $<$<CONFIG:Debug>:-pg>
    $<$<CONFIG:Release>:-flto>
)

target_link_libraries(cnes PRIVATE SDL3::SDL3 m)
