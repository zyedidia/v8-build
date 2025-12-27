cmake_minimum_required(VERSION 3.18)

# On Windows, use static runtime library to match V8
if(WIN32)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

project(v8_hello_world)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fetch V8 prebuilt binaries
include(FetchContent)

FetchContent_Declare(
  v8
  GIT_REPOSITORY https://github.com/zyedidia/v8-build.git
  GIT_TAG master
)

FetchContent_MakeAvailable(v8)

# Create executable and link against V8
add_executable(hello_world hello_world.cpp)
target_link_libraries(hello_world PRIVATE v8::v8)
