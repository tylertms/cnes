cmake_minimum_required(VERSION 3.13)
project(cnes LANGUAGES C)

include(FetchContent)
FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.2.x
)
FetchContent_MakeAvailable(SDL3)

add_executable(cnes
    src/apu.c
    src/cart.c
    src/cpu.c
    src/gui.c
    src/input.c
    src/main.c
    src/mapper.c
    src/nes.c
    src/ppu.c
)

set_property(TARGET cnes PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
set(CMAKE_OPTIMIZE_DEPENDENCIES TRUE)
target_compile_options(cnes PRIVATE
    "$<$<CONFIG:Release>:-O3>"
    "$<$<CONFIG:Release>:-ffast-math>"
    "$<$<CONFIG:Release>:-march=native>"
)

target_compile_options(cnes PRIVATE
    $<$<CONFIG:Debug>:-fsanitize=address>
)

target_link_options(cnes PRIVATE
    $<$<CONFIG:Debug>:-fsanitize=address>
)

target_link_libraries(cnes PRIVATE SDL3::SDL3 m)
