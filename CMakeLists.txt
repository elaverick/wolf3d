cmake_minimum_required(VERSION 3.21)

project(wolf3d_native C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

include(CTest)

option(WOLF3D_BUILD_SDL3_PLATFORM "Build the native SDL3 platform layer" ON)
option(WOLF3D_BUILD_PLATFORM_SMOKE_TEST "Build the SDL3 framebuffer smoke test" ON)
add_library(wolf3d_native_compat STATIC
    src/platform/native_compat.c
)

target_include_directories(wolf3d_native_compat
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src/platform
)

if(MSVC)
    target_compile_options(wolf3d_native_compat PRIVATE /W4 /WX)
else()
    target_compile_options(wolf3d_native_compat PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

option(WOLF3D_BUILD_LEGACY_GAME_OBJECTS "Compile portable original gameplay C sources" OFF)

add_library(wolf3d_video STATIC
    src/video/indexed_framebuffer.c
    src/video/vga_memory.c
    src/video/legacy_video.c
)

target_include_directories(wolf3d_video
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src/video
)

if(MSVC)
    target_compile_options(wolf3d_video PRIVATE /W4 /WX)
else()
    target_compile_options(wolf3d_video PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_library(wolf3d_legacy_vl STATIC
    src/video/legacy_vl.c
)

target_include_directories(wolf3d_legacy_vl
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src/video
)

target_link_libraries(wolf3d_legacy_vl
    PUBLIC
        wolf3d_video
)

if(MSVC)
    target_compile_options(wolf3d_legacy_vl PRIVATE /W4 /WX)
else()
    target_compile_options(wolf3d_legacy_vl PRIVATE -Wall -Wextra -Wpedantic -Werror)
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
        target_compile_options(test_indexed_framebuffer PRIVATE /W4 /WX)
    else()
        target_compile_options(test_indexed_framebuffer PRIVATE -Wall -Wextra -Wpedantic -Werror)
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
        target_compile_options(test_vga_memory PRIVATE /W4 /WX)
    else()
        target_compile_options(test_vga_memory PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()

    add_test(NAME vga_memory COMMAND test_vga_memory)

    add_executable(test_legacy_video
        tests/test_legacy_video.c
    )

    target_link_libraries(test_legacy_video
        PRIVATE
            wolf3d_video
    )

    if(MSVC)
        target_compile_options(test_legacy_video PRIVATE /W4 /WX)
    else()
        target_compile_options(test_legacy_video PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()

    add_test(NAME legacy_video COMMAND test_legacy_video)

    add_executable(test_legacy_vl
        tests/test_legacy_vl.c
    )

    target_link_libraries(test_legacy_vl
        PRIVATE
            wolf3d_legacy_vl
    )

    if(MSVC)
        target_compile_options(test_legacy_vl PRIVATE /W4 /WX)
    else()
        target_compile_options(test_legacy_vl PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()

    add_test(NAME legacy_vl COMMAND test_legacy_vl)

    add_executable(test_native_compat
        tests/test_native_compat.c
    )

    target_link_libraries(test_native_compat
        PRIVATE
            wolf3d_native_compat
    )

    if(MSVC)
        target_compile_options(test_native_compat PRIVATE /W4 /WX)
    else()
        target_compile_options(test_native_compat PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()

    add_test(NAME native_compat COMMAND test_native_compat)
endif()

if(WOLF3D_BUILD_LEGACY_GAME_OBJECTS)
    set(WOLF3D_GAME_SOURCES
        WOLFSRC/WL_ACT1.C
        WOLFSRC/WL_ACT2.C
        WOLFSRC/WL_AGENT.C
        WOLFSRC/WL_GAME.C
        WOLFSRC/WL_INTER.C
        WOLFSRC/WL_MENU.C
        WOLFSRC/WL_PLAY.C
        WOLFSRC/WL_STATE.C
        WOLFSRC/WL_TEXT.C
    )

    # Uppercase .C is normally treated as C++ by CMake on Unix.
    set_source_files_properties(${WOLF3D_GAME_SOURCES} PROPERTIES LANGUAGE C)

    add_library(wolf3d_game_objects OBJECT ${WOLF3D_GAME_SOURCES})
    target_include_directories(wolf3d_game_objects
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/WOLFSRC
            ${CMAKE_CURRENT_SOURCE_DIR}/src/video
            ${CMAKE_CURRENT_SOURCE_DIR}/src/platform
    )
    target_compile_definitions(wolf3d_game_objects PRIVATE WOLF3D_NATIVE)
    target_link_libraries(wolf3d_game_objects
        PRIVATE
            wolf3d_legacy_vl
            wolf3d_native_compat
    )

    if(MSVC)
        target_compile_options(wolf3d_game_objects PRIVATE /W4)
    else()
        target_compile_options(wolf3d_game_objects PRIVATE -Wall -Wextra -Wpedantic)
    endif()
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

        if(MSVC)
            target_compile_options(wolf3d_platform PRIVATE /W4 /WX)
        else()
            target_compile_options(wolf3d_platform PRIVATE -Wall -Wextra -Wpedantic -Werror)
        endif()

        if(WOLF3D_BUILD_PLATFORM_SMOKE_TEST)
            add_executable(wolf3d_platform_smoke
                examples/platform_smoke.c
            )

            target_link_libraries(wolf3d_platform_smoke
                PRIVATE
                    wolf3d_platform
                    wolf3d_legacy_vl
            )

            if(BUILD_TESTING)
                add_test(
                    NAME platform_smoke
                    COMMAND wolf3d_platform_smoke --frames 2
                )
                set_tests_properties(platform_smoke PROPERTIES
                    ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_RENDER_DRIVER=software"
                )
            endif()
        endif()
    else()
        message(STATUS "SDL3 was not found; the native platform targets are skipped.")
        message(STATUS "Install SDL3 development files or provide SDL3_DIR to enable them.")
    endif()
endif()
