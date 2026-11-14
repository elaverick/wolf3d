cmake_minimum_required(VERSION 3.21)

project(wolf3d_native C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(WOLF3D_BUILD_SDL3_PLATFORM "Build the native SDL3 platform layer" ON)

if(WOLF3D_BUILD_SDL3_PLATFORM)
    find_package(SDL3 CONFIG QUIET)

    if(SDL3_FOUND)
        add_library(wolf3d_platform STATIC
            src/platform/sdl3/platform_sdl3.c
        )

        target_include_directories(wolf3d_platform
            PUBLIC
                ${CMAKE_CURRENT_SOURCE_DIR}/src/platform
        )

        target_link_libraries(wolf3d_platform
            PUBLIC
                SDL3::SDL3
        )

        target_compile_definitions(wolf3d_platform
            PRIVATE
                SDL_MAIN_HANDLED
        )
    else()
        message(STATUS "SDL3 was not found; the native platform target is skipped.")
        message(STATUS "Install SDL3 development files or provide SDL3_DIR to enable it.")
    endif()
endif()
