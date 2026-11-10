#[=======================================================================[.rst:
cppref_exercise(<name> [LIBRARIES <libs>...])

Called from exercises/<dir>/CMakeLists.txt. Files are split by name:

* ``*_test.cpp`` / ``*_test.hpp`` in exercises/<dir>/ are the tests and
  test helpers (never replaced; in solutions mode compiled from copies in the
  build tree so quoted includes cannot reach the exercise's own headers).
* every other ``*.hpp`` / ``*.cpp`` is engineer-owned. They are taken from
  exercises/<dir>/, or from solutions/<dir>/ when CPPREF_SOLUTIONS is ON.

Creates library ``<name>`` (INTERFACE when header-only) and the GoogleTest
executable ``<name>_tests``; ctest test names are prefixed with ``<dir>.``.
Both get compile definitions with absolute paths for tests that read files:
``CPPREF_WORKDIR`` (engineer-owned files) and ``CPPREF_EXERCISE_DIR``
(provided assets).
#]=======================================================================]
function(cppref_exercise name)
  cmake_parse_arguments(PARSE_ARGV 1 ARG "" "" "LIBRARIES")
  get_filename_component(dir "${CMAKE_CURRENT_SOURCE_DIR}" NAME)
  set(exercise_dir "${CMAKE_CURRENT_SOURCE_DIR}")
  if(CPPREF_SOLUTIONS)
    set(work_dir "${PROJECT_SOURCE_DIR}/solutions/${dir}")
  else()
    set(work_dir "${exercise_dir}")
  endif()
  if(NOT IS_DIRECTORY "${work_dir}")
    message(FATAL_ERROR "cppref: missing ${work_dir}")
  endif()

  file(GLOB owned CONFIGURE_DEPENDS "${work_dir}/*.hpp" "${work_dir}/*.cpp")
  list(FILTER owned EXCLUDE REGEX "_test\\.(cpp|hpp)$")
  file(GLOB tests CONFIGURE_DEPENDS "${exercise_dir}/*_test.cpp" "${exercise_dir}/*_test.hpp")
  set(sources ${owned})
  list(FILTER sources INCLUDE REGEX "\\.cpp$")

  # `#include "x.hpp"` searches the including file's directory first, so tests
  # compiled in place would pick up exercises/<dir>/x.hpp even in solutions
  # mode. Compile copies from the build tree instead; there only the include
  # path (work_dir) can satisfy engineer-owned headers.
  if(CPPREF_SOLUTIONS)
    set(copied "")
    foreach(test IN LISTS tests)
      get_filename_component(test_name "${test}" NAME)
      set(copy "${CMAKE_CURRENT_BINARY_DIR}/cppref_tests/${test_name}")
      configure_file("${test}" "${copy}" COPYONLY)
      list(APPEND copied "${copy}")
    endforeach()
    set(tests ${copied})
  endif()

  set(defs CPPREF_WORKDIR="${work_dir}" CPPREF_EXERCISE_DIR="${exercise_dir}")
  if(sources)
    add_library(${name} STATIC ${owned})
    target_include_directories(${name} PUBLIC "${work_dir}")
    target_compile_definitions(${name} PUBLIC ${defs})
    target_link_libraries(${name} PUBLIC ${ARG_LIBRARIES})
  else()
    add_library(${name} INTERFACE ${owned})
    target_include_directories(${name} INTERFACE "${work_dir}")
    target_compile_definitions(${name} INTERFACE ${defs})
    target_link_libraries(${name} INTERFACE ${ARG_LIBRARIES})
  endif()

  add_executable(${name}_tests ${tests})
  target_link_libraries(${name}_tests PRIVATE ${name} GTest::gmock_main)
  gtest_discover_tests(${name}_tests
    TEST_PREFIX "${dir}."
    DISCOVERY_MODE PRE_TEST
    PROPERTIES TIMEOUT 60)
endfunction()
