cmake_minimum_required(VERSION 3.13)
project(cnes LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(CNES_BUILD_SHADERS "Rebuild shaders from source" ON)

set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON  CACHE BOOL "" FORCE)

include(FetchContent)
FetchContent_Declare(
    SDL3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG release-3.2.x
)
FetchContent_MakeAvailable(SDL3)

file(GLOB MAPPERS "${CMAKE_CURRENT_SOURCE_DIR}/src/mappers/*.c")

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


set(NES_SHADER_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
set(NES_SHADER_GEN_DIR ${NES_SHADER_SRC_DIR}/generated)

set(NES_VERT_SRC ${NES_SHADER_SRC_DIR}/nes.vert)
set(NES_FRAG_SRC ${NES_SHADER_SRC_DIR}/nes.frag)

set(NES_VERT_SPV_HDR ${NES_SHADER_GEN_DIR}/nes_vert_spv.h)
set(NES_FRAG_SPV_HDR ${NES_SHADER_GEN_DIR}/nes_frag_spv.h)
set(NES_VERT_MSL_HDR ${NES_SHADER_GEN_DIR}/nes_vert_msl.h)
set(NES_FRAG_MSL_HDR ${NES_SHADER_GEN_DIR}/nes_frag_msl.h)

if (CNES_BUILD_SHADERS)
    # Dev / local builds: regenerate shaders from GLSL
    add_custom_command(
        OUTPUT
            ${NES_VERT_SPV_HDR}
            ${NES_FRAG_SPV_HDR}
            ${NES_VERT_MSL_HDR}
            ${NES_FRAG_MSL_HDR}
        COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/tools/build_shaders.sh
        DEPENDS
            ${NES_VERT_SRC}
            ${NES_FRAG_SRC}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        VERBATIM
    )

    add_custom_target(shaders
        DEPENDS
            ${NES_VERT_SPV_HDR}
            ${NES_FRAG_SPV_HDR}
            ${NES_VERT_MSL_HDR}
            ${NES_FRAG_MSL_HDR}
    )

    add_dependencies(cnes shaders)
else()
    foreach(hdr IN ITEMS
        ${NES_VERT_SPV_HDR}
        ${NES_FRAG_SPV_HDR}
        ${NES_VERT_MSL_HDR}
        ${NES_FRAG_MSL_HDR}
    )
        if (NOT EXISTS "${hdr}")
            message(FATAL_ERROR "CNES_BUILD_SHADERS=OFF but missing pre-generated shader header: ${hdr}")
        endif()
    endforeach()
endif()


target_include_directories(cnes PRIVATE
    external/fonts
    external/imgui
    external/
    ${NES_SHADER_GEN_DIR}
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
        $<$<CONFIG:Release>:-fstrict-aliasing>
        $<$<CONFIG:Release>:-ffast-math>
    )
endif()

target_link_options(cnes PRIVATE
    $<$<CONFIG:Debug>:-fsanitize=address>
    $<$<CONFIG:Release>:-flto>
)

target_compile_definitions(cnes PRIVATE
    CMAKE_BUILD_TYPE_STR="$<CONFIG>"
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

set_property(TARGET cnes PROPERTY LINKER_LANGUAGE CXX)
target_link_libraries(cnes PRIVATE imgui)
