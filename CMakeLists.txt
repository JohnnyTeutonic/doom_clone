cmake_minimum_required(VERSION 3.10)
project(DoomClone VERSION 1.0)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Detect platform
if(WIN32)
    set(PLATFORM_WINDOWS TRUE)
else()
    set(PLATFORM_UNIX TRUE)
endif()

# Find SDL2 and related packages
if(PLATFORM_WINDOWS)
    # For Windows, you may need to set SDL2 path manually if CMake can't find it
    # set(SDL2_DIR "path/to/SDL2/cmake")
    
    # Find SDL2 in standard Windows locations
    find_package(SDL2 REQUIRED)
    include_directories(${SDL2_INCLUDE_DIRS})
    
    # For MinGW, we'll directly link against the libraries
    # Instead of using find_package which might not work correctly
    # Comment out the previous approach
    # set(SDL2_IMAGE_DIR "path/to/SDL2_image/cmake")
    # set(SDL2_TTF_DIR "path/to/SDL2_ttf/cmake")
    # find_package(SDL2_image REQUIRED)
    # find_package(SDL2_ttf REQUIRED)
else()
    # For Linux/WSL2
    find_package(SDL2 REQUIRED)
    include_directories(${SDL2_INCLUDE_DIRS})
    
    # For Linux/WSL2, we can use pkg-config if available
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(SDL2_IMAGE REQUIRED SDL2_image)
        pkg_check_modules(SDL2_TTF REQUIRED SDL2_ttf)
        pkg_check_modules(SDL2_MIXER REQUIRED SDL2_mixer)
    else()
        # Fallback for systems without pkg-config
        message(STATUS "pkg-config not found, using direct library linking for SDL2_image, SDL2_ttf, and SDL2_mixer")
    endif()
endif()

# Source files
set(SOURCES
    src/main.cpp
    src/engine.cpp
    src/renderer.cpp
    src/player.cpp
    src/map.cpp
    src/texture.cpp
    src/sprite.cpp
    src/projectile.cpp
    src/input.cpp
    src/utils.cpp
    src/audio.cpp
)

# Header files
set(HEADERS
    src/engine.h
    src/renderer.h
    src/player.h
    src/map.h
    src/texture.h
    src/sprite.h
    src/projectile.h
    src/input.h
    src/utils.h
    src/lighting.h
    src/audio.h
)

# Define executable
add_executable(doom_clone ${SOURCES} ${HEADERS})

# Define compile options
if(PLATFORM_WINDOWS)
    target_compile_definitions(doom_clone PRIVATE PLATFORM_WINDOWS)
    # Direct linking for MinGW/Windows
    target_link_libraries(doom_clone ${SDL2_LIBRARIES} SDL2_image SDL2_ttf SDL2_mixer)
else()
    target_compile_definitions(doom_clone PRIVATE PLATFORM_UNIX)
    target_link_libraries(doom_clone SDL2 SDL2_image SDL2_ttf SDL2_mixer SDL2main m)
endif()

# Copy assets to build directory
file(COPY assets DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}) 