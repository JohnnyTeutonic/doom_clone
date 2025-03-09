cmake_minimum_required(VERSION 3.10)
project(DoomClone VERSION 0.1.0)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add -O2 by default for Release builds
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2")

# On Windows, avoid MSYS path conversion issues
if(WIN32)
  set(CMAKE_MSYS_AVOID_PATH_CONVERSION 1)
endif()

# Find SDL2 packages
find_package(SDL2 REQUIRED)
find_package(SDL2_image REQUIRED)
find_package(SDL2_ttf REQUIRED)
find_package(SDL2_mixer REQUIRED)

# Include directories
include_directories(
    ${SDL2_INCLUDE_DIRS}
    ${SDL2_IMAGE_INCLUDE_DIRS}
    ${SDL2_TTF_INCLUDE_DIRS}
    ${SDL2_MIXER_INCLUDE_DIRS}
)

# Collect source files
file(GLOB_RECURSE SOURCE_FILES 
    src/*.cpp
)

# Collect header files
file(GLOB_RECURSE HEADER_FILES 
    src/*.h
)

# Add executable
add_executable(doom_clone ${SOURCE_FILES} ${HEADER_FILES})

# Link libraries
target_link_libraries(doom_clone
    ${SDL2_LIBRARIES}
    ${SDL2_IMAGE_LIBRARIES}
    ${SDL2_TTF_LIBRARIES}
    ${SDL2_MIXER_LIBRARIES}
)

# Copy assets to build directory
add_custom_command(TARGET doom_clone POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:doom_clone>/assets
)

# Set output directory
set_target_properties(doom_clone PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Windows specific configuration
if(WIN32)
    # Set the Windows subsystem to use GUI mode
    set_target_properties(doom_clone PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
    
    # Add the SDL2 DLLs to the output directory
    if(EXISTS ${SDL2_DIR}/bin/SDL2.dll)
        add_custom_command(TARGET doom_clone POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy
                ${SDL2_DIR}/bin/SDL2.dll
                $<TARGET_FILE_DIR:doom_clone>/SDL2.dll
        )
    endif()
    
    if(EXISTS ${SDL2_IMAGE_DIR}/bin/SDL2_image.dll)
        add_custom_command(TARGET doom_clone POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy
                ${SDL2_IMAGE_DIR}/bin/SDL2_image.dll
                $<TARGET_FILE_DIR:doom_clone>/SDL2_image.dll
        )
    endif()
    
    if(EXISTS ${SDL2_TTF_DIR}/bin/SDL2_ttf.dll)
        add_custom_command(TARGET doom_clone POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy
                ${SDL2_TTF_DIR}/bin/SDL2_ttf.dll
                $<TARGET_FILE_DIR:doom_clone>/SDL2_ttf.dll
        )
    endif()
    
    if(EXISTS ${SDL2_MIXER_DIR}/bin/SDL2_mixer.dll)
        add_custom_command(TARGET doom_clone POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy
                ${SDL2_MIXER_DIR}/bin/SDL2_mixer.dll
                $<TARGET_FILE_DIR:doom_clone>/SDL2_mixer.dll
        )
    endif()
endif()

# Add debug and optimization flags
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(doom_clone PRIVATE -g -Wall -Wextra)
    
    # Add address sanitizer in Debug mode
    if(NOT WIN32)
        target_compile_options(doom_clone PRIVATE -fsanitize=address)
        target_link_options(doom_clone PRIVATE -fsanitize=address)
    endif()
else()
    target_compile_options(doom_clone PRIVATE -O2)
endif()

# Output configuration summary
message(STATUS "Configuration:")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "  C++ flags: ${CMAKE_CXX_FLAGS}")
message(STATUS "  SDL2 include: ${SDL2_INCLUDE_DIRS}")
message(STATUS "  SDL2 libraries: ${SDL2_LIBRARIES}")
message(STATUS "  Output directory: ${CMAKE_BINARY_DIR}/bin") 