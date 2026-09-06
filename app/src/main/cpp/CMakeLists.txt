cmake_minimum_required(VERSION 3.22.1)
project(vfxengine CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(ENGINE_DEV_SHADER_HOTLOAD "Allow runtime GLSL->SPIR-V compilation (dev builds only)" OFF)

# ---------------------------------------------------------------------------
# Shader build step: compile GLSL -> SPIR-V and embed as a C++ header.
# ---------------------------------------------------------------------------
find_package(Python3 COMPONENTS Interpreter QUIET)

# Locate glslangValidator: first from ANDROID_NDK_HOME, then PATH.
set(NDK_GLSLANG_VALIDATOR "")
if(DEFINED ENV{ANDROID_NDK_HOME})
    set(NDK_GLSLANG_VALIDATOR
        "$ENV{ANDROID_NDK_HOME}/toolchains/llvm/prebuilt/linux-x86_64/bin/glslangValidator")
endif()

find_program(GLSLANG_VALIDATOR
    NAMES glslangValidator
    PATHS ${NDK_GLSLANG_VALIDATOR}
    PATH_SUFFIXES bin
    DOC "Path to glslangValidator for offline SPIR-V compilation"
)

set(SHADER_EMBEDDED_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/shaders_embedded.h")
set(SHADER_COMPILE_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/compile_shaders.py")

if(Python3_Interpreter_FOUND AND GLSLANG_VALIDATOR)
    add_custom_command(
        OUTPUT  ${SHADER_EMBEDDED_HEADER}
        COMMAND ${Python3_EXECUTABLE}
                ${SHADER_COMPILE_SCRIPT}
                --shader-dir "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
                --output ${SHADER_EMBEDDED_HEADER}
                --glslang-validator ${GLSLANG_VALIDATOR}
        DEPENDS ${SHADER_COMPILE_SCRIPT}
                ${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.vert
                ${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.frag
                ${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.comp
        COMMENT "Compiling GLSL shaders to embedded SPIR-V header"
        VERBATIM
    )
    message(STATUS "Shader compilation enabled: ${GLSLANG_VALIDATOR}")
else()
    message(STATUS "Shader compilation disabled (glslangValidator not found; shaders will be stubs)")
    # Create an empty placeholder header so the include still resolves.
    file(WRITE ${SHADER_EMBEDDED_HEADER} "// Shader compilation disabled: no glslangValidator found.\n")
endif()

# ---------------------------------------------------------------------------
# Library
# ---------------------------------------------------------------------------

add_library(vfxengine SHARED
    jni/jni_bridge.cpp
    Engine.cpp
    engine/vulkan/VulkanDevice.cpp
    engine/graph/RenderGraph.cpp
    engine/media/MediaEngine.cpp
    engine/core/Profiler.cpp
    engine/export/ExportPipeline.cpp
    engine/export/ProjectSerializer.cpp
    engine/text/TextRenderer.cpp
    engine/expression/ExpressionEngine.cpp
    engine/shape2d/Shape2D.cpp
    engine/audio/AudioEngine.cpp
    engine/asset/AssetManager.cpp
    engine/3d/Light.cpp
    engine/3d/Material.cpp
    engine/3d/Shadow.cpp
    engine/3d/glTFLoader.cpp
    ${SHADER_EMBEDDED_HEADER}
)

target_include_directories(vfxengine PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/engine
    ${CMAKE_CURRENT_BINARY_DIR}
)

if(ENGINE_DEV_SHADER_HOTLOAD)
    target_compile_definitions(vfxengine PRIVATE ENGINE_DEV_SHADER_HOTLOAD=1)
endif()

# Vulkan is provided by the NDK (libvulkan.so, loaded via android.hardware.vulkan).
find_library(log-lib log)
find_library(android-lib android)
find_library(vulkan-lib vulkan)
find_library(mediandk-lib mediandk)   # AMediaCodec / AMediaExtractor / AImageReader / AImage
find_library(nativewindow-lib nativewindow) # ANativeWindow / AHardwareBuffer

# HarfBuzz and FreeType for text shaping (NDK provides these)
find_library(harfbuzz-lib harfbuzz)
find_library(freetype-lib freetype)

target_link_libraries(vfxengine
    ${log-lib}
    ${android-lib}
    ${vulkan-lib}
    ${mediandk-lib}
    ${nativewindow-lib}
    ${harfbuzz-lib}
    ${freetype-lib}
)

target_compile_options(vfxengine PRIVATE
    -Wall -Wextra -Wno-unused-parameter
    $<$<CONFIG:Release>:-O3>
)
