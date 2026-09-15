cmake_minimum_required(VERSION 3.24)
project(willy CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(WIN32)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release)
endif()

# ============================================================================
# DEPENDENCIES
#   Windows: vcpkg CONFIG mode (SDL2, Freetype).
#   Linux:   native dev packages via pkg-config (matches compile.sh).
# ============================================================================
if(WIN32)
    find_package(SDL2 CONFIG REQUIRED)
    find_package(Freetype REQUIRED)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED sdl2)
    pkg_check_modules(FREETYPE REQUIRED freetype2)
endif()

# ============================================================================
# SOURCES (mirrors compile.sh)
# ============================================================================
set(SRCS
    willy_main.cpp wopr_willy.cpp wopr_render.cpp highscores.cpp
    miniz.c miniz_tdef.c miniz_tinfl.c miniz_zip.c
)

add_executable(willy WIN32 ${SRCS})

if(MSVC)
    target_compile_options(willy PRIVATE /MP)
endif()

target_include_directories(willy PRIVATE ${CMAKE_SOURCE_DIR})

if(WIN32)
    target_link_libraries(willy PRIVATE
        SDL2::SDL2main SDL2::SDL2
        Freetype::Freetype
    )
else()
    target_include_directories(willy PRIVATE ${SDL2_INCLUDE_DIRS} ${FREETYPE_INCLUDE_DIRS})
    target_link_libraries(willy PRIVATE ${SDL2_LIBRARIES} ${FREETYPE_LIBRARIES})
endif()
