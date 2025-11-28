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
    external/fonts
    external/imgui
    external/glad/include
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
        $<$<CONFIG:Debug>:-O0>
        $<$<CONFIG:Debug>:-g3>
        $<$<CONFIG:Debug>:-fsanitize=address>
        $<$<CONFIG:Debug>:-fno-omit-frame-pointer>
        $<$<CONFIG:Debug>:-fno-optimize-sibling-calls>
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
    $<$<CONFIG:Release>:-flto>
)

file(GLOB IMGUI_SRC
    ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui/backends/*.cpp
)
add_library(imgui STATIC ${IMGUI_SRC})
target_include_directories(imgui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/external/imgui
)
target_link_libraries(imgui PUBLIC SDL3::SDL3)

add_library(glad STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/external/glad/src/gl.c
)
target_include_directories(glad PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/external/glad/include
)
target_link_libraries(cnes PRIVATE glad)

set_property(TARGET cnes PROPERTY LINKER_LANGUAGE CXX)
target_link_libraries(cnes PRIVATE SDL3::SDL3 imgui m)
