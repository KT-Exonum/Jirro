cmake_minimum_required(VERSION 3.22.1)
project(vfxengine CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(ENGINE_DEV_SHADER_HOTLOAD "Allow runtime GLSL->SPIR-V compilation (dev builds only)" OFF)

add_library(vfxengine SHARED
    jni/jni_bridge.cpp
    Engine.cpp
    engine/vulkan/VulkanDevice.cpp
    engine/graph/RenderGraph.cpp
    engine/media/MediaEngine.cpp
)

target_include_directories(vfxengine PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/engine
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

target_link_libraries(vfxengine
    ${log-lib}
    ${android-lib}
    ${vulkan-lib}
    ${mediandk-lib}
    ${nativewindow-lib}
)

target_compile_options(vfxengine PRIVATE
    -Wall -Wextra -Wno-unused-parameter
    $<$<CONFIG:Release>:-O3>
)
