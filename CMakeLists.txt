cmake_minimum_required(VERSION 3.20)
project("VETi_HMD_Driver" LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ── Platform detection ──────────────────────────────────────────────────────
if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(PLATFORM_BITS "64")
else()
    set(PLATFORM_BITS "32")
endif()

if(WIN32)
    set(PLATFORM_NAME "win")
elseif(UNIX AND NOT APPLE)
    set(PLATFORM_NAME "linux")
elseif(APPLE)
    set(PLATFORM_NAME "osx")
endif()

set(ARCH_TARGET "${PLATFORM_NAME}${PLATFORM_BITS}")

# ── FetchContent: OpenVR SDK ────────────────────────────────────────────────
include(FetchContent)

FetchContent_Declare(
    openvr
    GIT_REPOSITORY https://github.com/ValveSoftware/openvr.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(openvr)

set(OPENVR_INCLUDE_DIR "${openvr_SOURCE_DIR}/headers")

if(WIN32)
    find_library(OPENVR_LIB openvr_api
        HINTS "${openvr_SOURCE_DIR}/lib/${ARCH_TARGET}/"
        NO_DEFAULT_PATH)
elseif(UNIX)
    find_library(OPENVR_LIB openvr_api
        HINTS "${openvr_SOURCE_DIR}/lib/${ARCH_TARGET}/"
              "${openvr_SOURCE_DIR}/bin/${ARCH_TARGET}/"
        NO_DEFAULT_PATH)
endif()

# ── FetchContent: HIDAPI ────────────────────────────────────────────────────
FetchContent_Declare(
    hidapi
    GIT_REPOSITORY https://github.com/libusb/hidapi.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)
set(HIDAPI_WITH_LIBUSB OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(hidapi)

# ── Driver target ───────────────────────────────────────────────────────────
set(DRIVER_NAME "VETi_hmd")

file(GLOB_RECURSE DRIVER_HEADERS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp")
file(GLOB_RECURSE DRIVER_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_library("driver_${DRIVER_NAME}" SHARED ${DRIVER_HEADERS} ${DRIVER_SOURCES})

target_include_directories("driver_${DRIVER_NAME}" PRIVATE
    "${OPENVR_INCLUDE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/utils/driverlog"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/utils/vrmath"
)

target_link_libraries("driver_${DRIVER_NAME}" PRIVATE ${OPENVR_LIB})

if(WIN32)
    target_link_libraries("driver_${DRIVER_NAME}" PRIVATE hidapi::winapi)
elseif(APPLE)
    target_link_libraries("driver_${DRIVER_NAME}" PRIVATE hidapi::hidapi)
else()
    target_link_libraries("driver_${DRIVER_NAME}" PRIVATE hidapi::hidraw)
endif()

target_compile_definitions("driver_${DRIVER_NAME}" PRIVATE
    $<$<PLATFORM_ID:Windows>:_USE_MATH_DEFINES>
)

# ── IDE source groups ───────────────────────────────────────────────────────
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src" PREFIX "Header Files" FILES ${DRIVER_HEADERS})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/src" PREFIX "Source Files" FILES ${DRIVER_SOURCES})

# ── Output: copy driver assets and binary into SteamVR-compatible layout ───
set(DRIVER_OUTPUT_DIR "${CMAKE_BINARY_DIR}/${DRIVER_NAME}")

add_custom_command(TARGET "driver_${DRIVER_NAME}" PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/driver/${DRIVER_NAME}"
        "${DRIVER_OUTPUT_DIR}"
)

add_custom_command(TARGET "driver_${DRIVER_NAME}" POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${DRIVER_OUTPUT_DIR}/bin/${ARCH_TARGET}"
    COMMAND ${CMAKE_COMMAND} -E copy
        "$<TARGET_FILE:driver_${DRIVER_NAME}>"
        "${DRIVER_OUTPUT_DIR}/bin/${ARCH_TARGET}/$<TARGET_FILE_NAME:driver_${DRIVER_NAME}>"
)

# On Windows, copy the HIDAPI DLL next to the driver
if(WIN32)
    add_custom_command(TARGET "driver_${DRIVER_NAME}" POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:hidapi::winapi>"
            "${DRIVER_OUTPUT_DIR}/bin/${ARCH_TARGET}/"
    )
endif()
