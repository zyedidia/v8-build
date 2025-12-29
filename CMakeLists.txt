cmake_minimum_required(VERSION 3.18)
project(v8_prebuilt VERSION 14.0.365.10)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# V8 version to download
set(V8_VERSION "v14.0.365.10-17" CACHE STRING "V8 version to download")
set(V8_REPO "zyedidia/v8-build" CACHE STRING "GitHub repository for V8 prebuilts")
option(V8_DEBUG "Download debug build of V8" OFF)
set(V8_LOCAL_PATH "" CACHE PATH "Path to local V8 install directory (skips download if set)")

# Determine build type
if(V8_DEBUG)
    set(V8_BUILD_TYPE "debug")
else()
    set(V8_BUILD_TYPE "release")
endif()

# Detect platform
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
        set(V8_PLATFORM "x86_64-unknown-linux-gnu")
        set(V8_LIB_NAME "libv8_monolith.a")
    elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
        set(V8_PLATFORM "aarch64-unknown-linux-gnu")
        set(V8_LIB_NAME "libv8_monolith.a")
    else()
        message(FATAL_ERROR "Unsupported Linux architecture: ${CMAKE_SYSTEM_PROCESSOR}")
    endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    if(CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
        set(V8_PLATFORM "aarch64-apple-darwin")
        set(V8_LIB_NAME "libv8_monolith.a")
    else()
        message(FATAL_ERROR "Unsupported macOS architecture: ${CMAKE_SYSTEM_PROCESSOR}")
    endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    if(CMAKE_SYSTEM_PROCESSOR STREQUAL "AMD64")
        set(V8_PLATFORM "x86_64-pc-windows-msvc")
        set(V8_LIB_NAME "v8_monolith.lib")
        if(V8_DEBUG)
            message(FATAL_ERROR "Debug builds are not available for Windows")
        endif()
    else()
        message(FATAL_ERROR "Unsupported Windows architecture: ${CMAKE_SYSTEM_PROCESSOR}")
    endif()
else()
    message(FATAL_ERROR "Unsupported platform: ${CMAKE_SYSTEM_NAME}")
endif()

# Use local path or download
if(V8_LOCAL_PATH)
    set(V8_DIR "${V8_LOCAL_PATH}")
    message(STATUS "Using local V8: ${V8_DIR}")
else()
    set(V8_URL "https://github.com/${V8_REPO}/releases/download/${V8_VERSION}/v8_${V8_BUILD_TYPE}_${V8_PLATFORM}.tar.gz")
    set(V8_DIR "${CMAKE_CURRENT_BINARY_DIR}/v8")
    set(V8_ARCHIVE "${V8_DIR}/v8.tar.gz")

    if(NOT EXISTS "${V8_DIR}/${V8_LIB_NAME}")
        message(STATUS "Downloading V8 ${V8_VERSION} (${V8_BUILD_TYPE}) for ${V8_PLATFORM}...")
        file(DOWNLOAD "${V8_URL}" "${V8_ARCHIVE}"
             SHOW_PROGRESS
             STATUS DOWNLOAD_STATUS)

        list(GET DOWNLOAD_STATUS 0 STATUS_CODE)
        if(NOT STATUS_CODE EQUAL 0)
            list(GET DOWNLOAD_STATUS 1 ERROR_MESSAGE)
            message(FATAL_ERROR "Failed to download V8: ${ERROR_MESSAGE}")
        endif()

        message(STATUS "Extracting V8...")
        file(ARCHIVE_EXTRACT
             INPUT "${V8_ARCHIVE}"
             DESTINATION "${V8_DIR}")

        file(REMOVE "${V8_ARCHIVE}")
    endif()
endif()

# Find V8 library
find_library(V8_LIBRARY
    NAMES ${V8_LIB_NAME}
    PATHS "${V8_DIR}"
    NO_DEFAULT_PATH
    REQUIRED)

# On Linux, also find additional V8 libraries that may not be fully merged into monolith
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_library(V8_LIBBASE
        NAMES libv8_libbase.a
        PATHS "${V8_DIR}"
        NO_DEFAULT_PATH
        REQUIRED)
    find_library(V8_LIBPLATFORM
        NAMES libv8_libplatform.a
        PATHS "${V8_DIR}"
        NO_DEFAULT_PATH
        REQUIRED)
endif()

# Create imported library target
add_library(v8_monolith STATIC IMPORTED GLOBAL)
set_target_properties(v8_monolith PROPERTIES
    IMPORTED_LOCATION "${V8_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${V8_DIR}/include"
)

# Debug build definitions
if(V8_DEBUG)
    set_property(TARGET v8_monolith APPEND PROPERTY
        INTERFACE_COMPILE_DEFINITIONS V8_ENABLE_CHECKS)
endif()

# Platform-specific link libraries
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set_property(TARGET v8_monolith APPEND PROPERTY
        INTERFACE_LINK_LIBRARIES "${V8_LIBBASE}" "${V8_LIBPLATFORM}" pthread dl atomic)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set_property(TARGET v8_monolith APPEND PROPERTY
        INTERFACE_LINK_LIBRARIES
        "-framework CoreFoundation"
        "-framework Foundation"
        "-framework Security")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    set_property(TARGET v8_monolith APPEND PROPERTY
        INTERFACE_LINK_LIBRARIES winmm dbghelp)
endif()

# Create alias for namespaced target
add_library(v8::v8 ALIAS v8_monolith)

message(STATUS "V8 platform: ${V8_PLATFORM}")
message(STATUS "V8 build type: ${V8_BUILD_TYPE}")
message(STATUS "V8 library: ${V8_LIBRARY}")
