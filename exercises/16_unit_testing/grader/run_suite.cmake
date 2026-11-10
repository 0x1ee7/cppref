# Runs a grader check on the suite executable.
#   cmake -DEXE=<path> -DMIN_TESTS=<n> -P run_suite.cmake
# Fails if the suite has fewer than MIN_TESTS tests or if any test fails.

if(NOT DEFINED EXE OR NOT DEFINED MIN_TESTS)
  message(FATAL_ERROR "run_suite.cmake: EXE and MIN_TESTS are required")
endif()

execute_process(
  COMMAND "${EXE}" --gtest_list_tests
  OUTPUT_VARIABLE listing
  ERROR_VARIABLE listing_err
  RESULT_VARIABLE list_rc)
if(NOT list_rc EQUAL 0)
  message(FATAL_ERROR "cannot list tests (exit ${list_rc}): ${listing_err}")
endif()

# Test lines are indented; suite lines are not.
string(REGEX MATCHALL "\n  [^\n]+" test_lines "${listing}")
list(LENGTH test_lines test_count)
if(test_count LESS MIN_TESTS)
  message(FATAL_ERROR "suite has ${test_count} tests; at least ${MIN_TESTS} required")
endif()
message(STATUS "suite lists ${test_count} tests (minimum ${MIN_TESTS})")

execute_process(
  COMMAND "${EXE}"
  OUTPUT_VARIABLE run_out
  ERROR_VARIABLE run_err
  RESULT_VARIABLE run_rc)
if(NOT run_rc EQUAL 0)
  message(FATAL_ERROR "suite failed (exit ${run_rc}):\n${run_out}${run_err}")
endif()
