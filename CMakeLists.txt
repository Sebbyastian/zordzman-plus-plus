cmake_minimum_required(VERSION 3.0)

project(zordzman)

# Detect compiler
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    set(USING_GCC TRUE)
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    set(USING_CLANG TRUE)
endif()

include_directories(common/extlib/json11)
include_directories(common/extlib/cppformat)
include_directories(common/extlib/base64)

add_library(cppformat common/extlib/cppformat/format.cc)

# For GCC and Clang, enable C++11 support and add some other flags
if(USING_GCC OR USING_CLANG OR GATSAN)
  add_compile_options(-std=c++11 -pedantic -Wall -Wextra
                      -Wmissing-declarations -Wnon-virtual-dtor)
  if(SANITIZE)
    add_compile_options(-fsanitize=${SANITIZE})
    set(CMAKE_EXE_LINKER_FLAGS
        "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=${SANITIZE}")
  endif()
endif()

add_library(json11 common/extlib/json11/json11.cpp)
add_library(hash-library common/extlib/hash-library/md5.cpp)

add_library(server server/lib/Server.cpp)
add_library(logger common/logger/Logger.hpp common/logger/Logger.cpp)
file(GLOB_RECURSE COMMON_NET_SOURCES common/net/*.*pp)
add_library(common_net ${COMMON_NET_SOURCES})
file(GLOB_RECURSE COMMON_UTIL_SOURCES common/util/*.*pp)
add_library(common_util ${COMMON_UTIL_SOURCES})
add_library(zjson common/zjson/zjson.hpp common/zjson/zjson.cpp)
add_library(base64
            common/extlib/base64/base64.hpp common/extlib/base64/base64.cpp)

set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/CMake/Modules)

find_package(OpenGL REQUIRED)
include_directories(${OPENGL_INCLUDE_DIR})
find_package(SDL2 REQUIRED)
include_directories(${SDL2_INCLUDE_DIR})
find_package(SDL2_image REQUIRED)
include_directories(${SDLIMAGE_INCLUDE_DIR})
find_package(SDL2_mixer REQUIRED)
include_directories(${SDLMIXER_INCLUDE_DIR})

if(OSX)
    set(SDLIMAGE_LIBRARY /Library/Frameworks/SDL2_image.framework)
    set(SDLIMAGE_INCLUDE_DIR /Library/Frameworks/SDL2_image.framework/Headers)
    set(SDLMIXER_LIBRARY /Library/Frameworks/SDL2_mixer.framework)
    set(SDLMIXER_INCLUDE_DIR /Library/Frameworks/SDL2_mixer.framework/Headers)
    add_compile_options(-stdlib=libc++)
    add_compile_options(-std=c++11)
endif()

include_directories(.)
include_directories(client)

file(GLOB_RECURSE CLIENT_SOURCES client/*.*pp)
add_executable(zordzman ${CLIENT_SOURCES})

file(GLOB_RECURSE SERVER_SOURCES server/*.*pp)
add_executable(zordzman-server ${SERVER_SOURCES})

target_link_libraries(zordzman
    ${SDL2_LIBRARY}
    ${SDLIMAGE_LIBRARY}
    ${OPENGL_LIBRARY}
    ${SDLMIXER_LIBRARY}
    json11
    cppformat
    server
    zjson
    hash-library
    common_util
    common_net
)

target_link_libraries(zordzman-server
    ${SDL2_LIBRARY}
    server
    json11
    cppformat
    logger
    zjson
    base64
    hash-library
    common_util
    common_net
)
