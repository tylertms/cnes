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
    src/input.c
    src/main.c
    src/mapper.c
    src/nes.c
    src/ppu.c
)

target_link_libraries(cnes PRIVATE SDL3::SDL3)
