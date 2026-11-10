cmake_minimum_required(VERSION 3.28)
project(cppref LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(CPPREF_SOLUTIONS "Build solutions/<exercise> sources instead of exercises/<exercise>" OFF)
set(CPPREF_ONLY "" CACHE STRING "Only configure these exercise directories (;-separated), e.g. 05_iterators_and_ranges")

if(MSVC)
  add_compile_options(/W4 /permissive- /utf-8 /Zc:__cplusplus /Zc:preprocessor /EHsc)
else()
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()
include(GoogleTest)
include(cmake/cppref.cmake)

file(GLOB exercise_dirs LIST_DIRECTORIES true RELATIVE "${PROJECT_SOURCE_DIR}/exercises" "${PROJECT_SOURCE_DIR}/exercises/*")
list(SORT exercise_dirs)
foreach(dir IN LISTS exercise_dirs)
  if(NOT EXISTS "${PROJECT_SOURCE_DIR}/exercises/${dir}/CMakeLists.txt")
    continue()
  endif()
  if(CPPREF_ONLY AND NOT dir IN_LIST CPPREF_ONLY)
    continue()
  endif()
  add_subdirectory("exercises/${dir}")
endforeach()
