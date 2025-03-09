cmake_minimum_required(VERSION 3.10)
project(DoomClone VERSION 1.0 LANGUAGES CXX CUDA)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable SIMD instruction sets
if(MSVC)
    # MSVC automatically detects and uses the highest available instruction set
    # But we can explicitly enable specific sets if needed
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /arch:SSE2")
else()
    # GCC and Clang
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -msse2")
    # Optional: Enable AVX if available
    # set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx")
endif()

# Set CUDA standard
set(CMAKE_CUDA_STANDARD 14)
set(CMAKE_CUDA_STANDARD_REQUIRED ON)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Detect platform
if(WIN32)
    set(PLATFORM_WINDOWS TRUE)
else()
    set(PLATFORM_UNIX TRUE)
endif()

# Find CUDA
find_package(CUDA REQUIRED)
include_directories(${CUDA_INCLUDE_DIRS})

# Set libwebp path - adjust this to your actual path
set(LIBWEBP_DIR "C:/Libraries/libwebp")
if(EXISTS "${LIBWEBP_DIR}")
    message(STATUS "Found libwebp at ${LIBWEBP_DIR}")
    include_directories("${LIBWEBP_DIR}/include")
    link_directories("${LIBWEBP_DIR}/lib")
    add_definitions(-DUSE_LIBWEBP)
else()
    # Try to find libwebp using pkg-config
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(LIBWEBP libwebp)
        pkg_check_modules(LIBWEBPDEMUX libwebpdemux)
        
        if(LIBWEBP_FOUND AND LIBWEBPDEMUX_FOUND)
            message(STATUS "Found libwebp using pkg-config")
            include_directories(${LIBWEBP_INCLUDE_DIRS} ${LIBWEBPDEMUX_INCLUDE_DIRS})
            link_directories(${LIBWEBP_LIBRARY_DIRS} ${LIBWEBPDEMUX_LIBRARY_DIRS})
            add_definitions(-DUSE_LIBWEBP)
        else()
            message(WARNING "libwebp not found using pkg-config, animated webp support will be limited")
        endif()
    else()
        message(WARNING "pkg-config not found, animated webp support will be limited")
    endif()
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

# CUDA source files
set(CUDA_SOURCES
    src/cuda_renderer.cu
)

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
    src/cuda_renderer.cpp
    src/webp_loader.cpp
)

# Header files
set(HEADERS
    src/engine.h
    src/game_state.h
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
    src/cuda_renderer.h
)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pg")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -pg")

# Define executable
add_executable(doom_clone ${SOURCES} ${CUDA_SOURCES} ${HEADERS})

# Set CUDA specific flags
set_target_properties(doom_clone PROPERTIES
    CUDA_SEPARABLE_COMPILATION ON
)

# Define compile options
if(PLATFORM_WINDOWS)
    target_compile_definitions(doom_clone PRIVATE PLATFORM_WINDOWS)
    # Direct linking for MinGW/Windows
    if(EXISTS "${LIBWEBP_DIR}")
        target_link_libraries(doom_clone ${SDL2_LIBRARIES} SDL2_image SDL2_ttf SDL2_mixer ${CUDA_LIBRARIES} ${CUDA_CUDA_LIBRARY} ${CUDA_CUDART_LIBRARY} webp webpdemux)
    else()
        # Try pkg-config libraries
        if(LIBWEBP_FOUND AND LIBWEBPDEMUX_FOUND)
            target_link_libraries(doom_clone ${SDL2_LIBRARIES} SDL2_image SDL2_ttf SDL2_mixer ${CUDA_LIBRARIES} ${CUDA_CUDA_LIBRARY} ${CUDA_CUDART_LIBRARY} ${LIBWEBP_LIBRARIES} ${LIBWEBPDEMUX_LIBRARIES})
        else()
            target_link_libraries(doom_clone ${SDL2_LIBRARIES} SDL2_image SDL2_ttf SDL2_mixer ${CUDA_LIBRARIES} ${CUDA_CUDA_LIBRARY} ${CUDA_CUDART_LIBRARY})
        endif()
    endif()
else()
    target_compile_definitions(doom_clone PRIVATE PLATFORM_UNIX)
    if(EXISTS "${LIBWEBP_DIR}")
        target_link_libraries(doom_clone SDL2 SDL2_image SDL2_ttf SDL2_mixer SDL2main m ${CUDA_LIBRARIES} ${CUDA_CUDA_LIBRARY} ${CUDA_CUDART_LIBRARY} webp webpdemux)
    else()
        # Try pkg-config libraries
        if(LIBWEBP_FOUND AND LIBWEBPDEMUX_FOUND)
            target_link_libraries(doom_clone SDL2 SDL2_image SDL2_ttf SDL2_mixer SDL2main m ${CUDA_LIBRARIES} ${CUDA_CUDA_LIBRARY} ${CUDA_CUDART_LIBRARY} ${LIBWEBP_LIBRARIES} ${LIBWEBPDEMUX_LIBRARIES})
        else()
            target_link_libraries(doom_clone SDL2 SDL2_image SDL2_ttf SDL2_mixer SDL2main m ${CUDA_LIBRARIES} ${CUDA_CUDA_LIBRARY} ${CUDA_CUDART_LIBRARY})
        endif()
    endif()
endif()

# Copy assets to build directory
file(COPY assets DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}) 