cmake_minimum_required(VERSION 3.21)

project(wolf3d_native C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

include(CTest)

option(WOLF3D_BUILD_SDL3_PLATFORM "Build the native SDL3 platform layer" ON)
option(WOLF3D_BUILD_PLATFORM_SMOKE_TEST "Build the SDL3 framebuffer smoke test" ON)

add_library(wolf3d_video STATIC
    src/video/indexed_framebuffer.c
    src/video/vga_memory.c
)

target_include_directories(wolf3d_video
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src/video
)

if(MSVC)
    target_compile_options(wolf3d_video PRIVATE /W4)
else()
    target_compile_options(wolf3d_video PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(BUILD_TESTING)
    add_executable(test_indexed_framebuffer
        tests/test_indexed_framebuffer.c
    )

    target_link_libraries(test_indexed_framebuffer
        PRIVATE
            wolf3d_video
    )

    if(MSVC)
        target_compile_options(test_indexed_framebuffer PRIVATE /W4)
    else()
        target_compile_options(test_indexed_framebuffer PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_test(NAME indexed_framebuffer COMMAND test_indexed_framebuffer)

    add_executable(test_vga_memory
        tests/test_vga_memory.c
    )

    target_link_libraries(test_vga_memory
        PRIVATE
            wolf3d_video
    )

    if(MSVC)
        target_compile_options(test_vga_memory PRIVATE /W4)
    else()
        target_compile_options(test_vga_memory PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_test(NAME vga_memory COMMAND test_vga_memory)
endif()

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

        if(WOLF3D_BUILD_PLATFORM_SMOKE_TEST)
            add_executable(wolf3d_platform_smoke
                examples/platform_smoke.c
            )

            target_link_libraries(wolf3d_platform_smoke
                PRIVATE
                    wolf3d_platform
            )
        endif()
    else()
        message(STATUS "SDL3 was not found; the native platform targets are skipped.")
        message(STATUS "Install SDL3 development files or provide SDL3_DIR to enable them.")
    endif()
endif()
