cmake_minimum_required(VERSION 3.15)
project(clodbot-v2 C)

set(CMAKE_C_STANDARD 11)

option(RASPI "Build for Raspberry Pi (DRM/GLES backend)" OFF)

add_executable(${PROJECT_NAME} main.c video.c control.c settings.c)

# ----------------------------------------
# Normal desktop build (raylib via CMake)
# ----------------------------------------

if(NOT RASPI)
find_package(raylib REQUIRED)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBNM REQUIRED IMPORTED_TARGET libnm)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        raylib
        m
        vlc
        PkgConfig::LIBNM
)

# ----------------------------------------
# Raspberry Pi build (manual linking)
# ----------------------------------------

else()
message(STATUS "Building in Raspberry Pi mode")

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBNM REQUIRED IMPORTED_TARGET libnm)

# raylib itself
target_link_libraries(${PROJECT_NAME}
    PRIVATE raylib
)

# Graphics stack
target_link_libraries(${PROJECT_NAME}
    PRIVATE
        GLESv2
        EGL
        drm
        gbm
        vlc
)

# System libs + NetworkManager (via pkg-config)
target_link_libraries(${PROJECT_NAME}
    PRIVATE
        pthread
        dl
        m
        PkgConfig::LIBNM
)

endif()

