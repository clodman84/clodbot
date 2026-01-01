cmake_minimum_required(VERSION 3.15)
project(clodbot-v2 C)

set(CMAKE_C_STANDARD 11)

option(RASPI "Build for Raspberry Pi (DRM/GLES backend)" OFF)

add_executable(${PROJECT_NAME}
    main.c
    video.c
    control.c
)

# ----------------------------------------
# Normal desktop build (raylib via CMake)
# ----------------------------------------
if(NOT RASPI)
    find_package(raylib REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE raylib m vlc)

# ----------------------------------------
# Raspberry Pi build (manual linking)
# ----------------------------------------
else()
    message(STATUS "Building in Raspberry Pi mode")

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

    # System libs
    target_link_libraries(${PROJECT_NAME}
        PRIVATE
            pthread
            dl
            m
    )
endif()

