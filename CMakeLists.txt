set (MRPC_VERSION_MAJOR 1)
set (MRPC_VERSION_MINOR 0)

cmake_minimum_required (VERSION 3.5 FATAL_ERROR)
project(mrpc) # 项目名称

IF (CMAKE_SYSTEM_NAME MATCHES "Linux")
  set(CMAKE_CXX_COMPILER"clang++")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -std=c++2a -stdlib=libc++ -lc++abi -lpthread")
ELSEIF (CMAKE_SYSTEM_NAME MATCHES "Windows")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++latest /await ")
ENDIF (CMAKE_SYSTEM_NAME MATCHES "Linux")

set(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/libs)
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)

add_subdirectory(src)
add_subdirectory(test)

add_subdirectory(third_party/spdlog)
add_subdirectory(third_party/fmtlib)