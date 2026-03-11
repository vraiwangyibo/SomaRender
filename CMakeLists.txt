cmake_minimum_required(VERSION 3.18)
project(SomaRender LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)

# GLFW (window)
find_package(glfw3 REQUIRED)

# Dawn (WebGPU native). Set DAWN_DIR to Dawn build or install prefix if not in system.
if(DEFINED ENV{DAWN_DIR})
  set(DAWN_DIR "$ENV{DAWN_DIR}")
endif()
if(DAWN_DIR)
  list(APPEND CMAKE_PREFIX_PATH "${DAWN_DIR}")
endif()
find_package(dawn REQUIRED)

add_executable(SomaRender
  src/main.cpp
  src/gpu_context.cpp
  src/volume_loader.cpp
  src/camera.cpp
)

target_include_directories(SomaRender PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(SomaRender PRIVATE
  glfw
  dawn::dawn
)

# Copy shaders to build dir so runtime can find them
add_custom_command(TARGET SomaRender POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/shaders
    $<TARGET_FILE_DIR:SomaRender>/shaders
)
