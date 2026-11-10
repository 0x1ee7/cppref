find_package(Threads REQUIRED)

# Build metadata stamped into the binary. Override on the command line, e.g.
#   -DCPPREF_BUILD_VERSION=1.4.2 -DCPPREF_BUILD_GIT_SHA=abc1234
# An empty CPPREF_BUILD_GIT_SHA means "ask git", falling back to "unknown".
set(CPPREF_BUILD_VERSION "0.1.0" CACHE STRING "Version stamped into cppref-service")
set(CPPREF_BUILD_GIT_SHA "" CACHE STRING "Git SHA stamped into cppref-service (empty = ask git)")

cppref_exercise(ex18_build_and_deployment LIBRARIES Threads::Threads)

if(CPPREF_BUILD_GIT_SHA)
  set(ex18_git_sha "${CPPREF_BUILD_GIT_SHA}")
else()
  execute_process(
    COMMAND git rev-parse --short=12 HEAD
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    OUTPUT_VARIABLE ex18_git_out
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
    RESULT_VARIABLE ex18_git_rc)
  if(ex18_git_rc EQUAL 0 AND NOT ex18_git_out STREQUAL "")
    set(ex18_git_sha "${ex18_git_out}")
  else()
    set(ex18_git_sha "unknown")
  endif()
endif()

target_compile_definitions(ex18_build_and_deployment PUBLIC
  "CPPREF_BUILD_VERSION=\"${CPPREF_BUILD_VERSION}\""
  "CPPREF_BUILD_GIT_SHA=\"${ex18_git_sha}\"")

# The service binary. Its entry point lives in app/ (not globbed by the harness)
# and is taken from solutions/ in solutions mode, like the other owned files.
if(CPPREF_SOLUTIONS)
  set(ex18_work_dir "${PROJECT_SOURCE_DIR}/solutions/18_build_and_deployment")
else()
  set(ex18_work_dir "${CMAKE_CURRENT_SOURCE_DIR}")
endif()

add_executable(ex18_cli "${ex18_work_dir}/app/main.cpp")
target_link_libraries(ex18_cli PRIVATE ex18_build_and_deployment)
add_dependencies(ex18_build_and_deployment_tests ex18_cli)
target_compile_definitions(ex18_build_and_deployment_tests PRIVATE
  "CPPREF_CLI_PATH=\"$<TARGET_FILE:ex18_cli>\"")
