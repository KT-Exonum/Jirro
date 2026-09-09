cmake_minimum_required(VERSION 3.22.1)
project(vfxengine CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(ENGINE_DEV_SHADER_HOTLOAD "Allow runtime GLSL->SPIR-V compilation (dev builds only)" OFF)
option(ENGINE_ENABLE_TEXT_RENDERING "Enable text rendering with HarfBuzz+FreeType" OFF)
option(ENGINE_ENABLE_EXPRESSION_ENGINE "Enable expression engine with QuickJS" OFF)

# ---------------------------------------------------------------------------
# Shader build step: compile GLSL -> SPIR-V and embed as a C++ header.
# ---------------------------------------------------------------------------
find_package(Python3 COMPONENTS Interpreter QUIET)

# Locate glslangValidator: first from ANDROID_NDK_HOME, then VULKAN_SDK, then PATH.
set(NDK_GLSLANG_VALIDATOR "")
if(DEFINED ENV{ANDROID_NDK_HOME})
    if(WIN32)
        set(_NDK_PREBUILT "$ENV{ANDROID_NDK_HOME}/toolchains/llvm/prebuilt/windows-x86_64")
    else()
        set(_NDK_PREBUILT "$ENV{ANDROID_NDK_HOME}/toolchains/llvm/prebuilt/linux-x86_64")
    endif()
    set(NDK_GLSLANG_VALIDATOR "${_NDK_PREBUILT}/bin/glslangValidator")
endif()

find_program(GLSLANG_VALIDATOR
    NAMES glslangValidator
    PATHS ${NDK_GLSLANG_VALIDATOR}
          "$ENV{VULKAN_SDK}/Bin"
    PATH_SUFFIXES bin
    DOC "Path to glslangValidator for offline SPIR-V compilation"
)

set(SHADER_EMBEDDED_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/shaders_embedded.h")
set(SHADER_COMPILE_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/compile_shaders.py")

file(GLOB SHADER_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.vert"
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.frag"
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders/*.comp"
)

if(Python3_Interpreter_FOUND AND GLSLANG_VALIDATOR)
    add_custom_command(
        OUTPUT  ${SHADER_EMBEDDED_HEADER}
        COMMAND ${Python3_EXECUTABLE}
                ${SHADER_COMPILE_SCRIPT}
                --shader-dir "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
                --output ${SHADER_EMBEDDED_HEADER}
                --glslang-validator ${GLSLANG_VALIDATOR}
        DEPENDS ${SHADER_COMPILE_SCRIPT}
                ${SHADER_SOURCES}
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
    engine/core/RuntimeShaderCompiler.cpp
    engine/vulkan/VulkanDevice.cpp
    engine/graph/RenderGraph.cpp
    engine/media/MediaEngine.cpp
    engine/core/Profiler.cpp
    engine/export/ExportPipeline.cpp
    engine/export/ProjectSerializer.cpp
    engine/audio/AudioEngine.cpp
    engine/audio/AudioOutput.cpp
    engine/asset/AssetManager.cpp
    engine/3d/Light.cpp
    engine/3d/Material.cpp
    engine/3d/Shadow.cpp
    engine/3d/glTFLoader.cpp
    ${SHADER_EMBEDDED_HEADER}
)

if(ENGINE_ENABLE_TEXT_RENDERING)
    target_sources(vfxengine PRIVATE engine/text/TextRenderer.cpp)
    target_compile_definitions(vfxengine PRIVATE ENGINE_ENABLE_TEXT_RENDERING=1)
endif()

if(ENGINE_ENABLE_EXPRESSION_ENGINE)
    target_sources(vfxengine PRIVATE engine/expression/ExpressionEngine.cpp)
    target_compile_definitions(vfxengine PRIVATE ENGINE_ENABLE_EXPRESSION_ENGINE=1)
endif()

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
find_library(aaudio-lib aaudio)       # AAudio for low-latency audio output

# HarfBuzz and FreeType for text shaping (optional - may not be in all NDK versions)
find_library(harfbuzz-lib harfbuzz)
find_library(freetype-lib freetype)

set(TEXT_LIBS "")
if(harfbuzz-lib)
    list(APPEND TEXT_LIBS ${harfbuzz-lib})
endif()
if(freetype-lib)
    list(APPEND TEXT_LIBS ${freetype-lib})
endif()

target_link_libraries(vfxengine
    ${log-lib}
    ${android-lib}
    ${vulkan-lib}
    ${mediandk-lib}
    ${nativewindow-lib}
    ${aaudio-lib}
    ${TEXT_LIBS}
)

target_compile_options(vfxengine PRIVATE
    -Wall -Wextra -Wno-unused-parameter
    $<$<CONFIG:Release>:-O3>
)
