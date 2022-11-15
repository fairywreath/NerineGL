if (POLICY CMP0048)
  cmake_policy(SET CMP0048 NEW)
endif()

cmake_minimum_required(VERSION 3.16)

project(Nerine VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20) 

if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL 19.12.25835)
   set(CMAKE_CXX20_STANDARD_COMPILE_OPTION "-std:c++latest")
   set(CMAKE_CXX20_EXTENSION_COMPILE_OPTION "-std:c++latest")
endif()

set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/../)

add_subdirectory(Source)