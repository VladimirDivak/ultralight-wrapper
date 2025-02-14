cmake_minimum_required(VERSION 3.10)
project(UltralightWrapper)

file(GLOB SOURCES "src/*.cpp")
include_directories("src")

set(ULTRALIGHT_DIR "ultralight")
include_directories(${ULTRALIGHT_DIR}/include)
link_directories(${ULTRALIGHT_DIR}/lib)
set(CMAKE_BUILD_TYPE Release)
add_library(UltralightWrapper SHARED ${SOURCES})
target_link_libraries(UltralightWrapper AppCore WebCore UltralightCore Ultralight d3d11 dxgi d3dcompiler)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY D:/UnityProjects/Ultralight UI/Assets/Plugins)