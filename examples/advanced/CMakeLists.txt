cmake_minimum_required(VERSION 3.18)

if(WIN32)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

project(v8_advanced_examples)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

FetchContent_Declare(
  v8
  GIT_REPOSITORY https://github.com/zyedidia/v8-build.git
  GIT_TAG master
)

FetchContent_MakeAvailable(v8)

set(EXAMPLES
  callbacks
  globals
  objects
  exceptions
  wrap_class
  call_js
)

foreach(example ${EXAMPLES})
  add_executable(${example} ${example}.cc)
  target_link_libraries(${example} PRIVATE v8::v8)
endforeach()
