# 16 · Unit testing: the engineer writes student_suite.cpp against billing/.
#
# Custom CMake instead of cppref_exercise(): billing/ is provided and must not
# be swapped, and the grader needs one suite object linked against the real
# billing.cpp and against generated mutants of it.

set(prefix "16_unit_testing.")
set(exercise_dir "${CMAKE_CURRENT_SOURCE_DIR}")
get_filename_component(dir "${exercise_dir}" NAME)

if(CPPREF_SOLUTIONS)
  set(suite_src "${PROJECT_SOURCE_DIR}/solutions/${dir}/student_suite.cpp")
else()
  set(suite_src "${exercise_dir}/student_suite.cpp")
endif()
if(NOT EXISTS "${suite_src}")
  message(FATAL_ERROR "cppref: missing ${suite_src}")
endif()

set(billing_src "${exercise_dir}/billing/billing.cpp")
set(grader_script "${exercise_dir}/grader/run_suite.cmake")
set(mutant_dir "${CMAKE_CURRENT_BINARY_DIR}/mutants")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${billing_src}")

# The suite is compiled once and linked into the baseline and every mutant.
add_library(ex16_student_suite OBJECT "${suite_src}")
target_include_directories(ex16_student_suite PRIVATE "${exercise_dir}")
target_link_libraries(ex16_student_suite PRIVATE GTest::gmock)

add_library(ex16_billing STATIC "${billing_src}")
target_include_directories(ex16_billing PUBLIC "${exercise_dir}")

# Baseline: suite + unmodified billing.
add_executable(ex16_unit_testing_tests $<TARGET_OBJECTS:ex16_student_suite>)
target_link_libraries(ex16_unit_testing_tests PRIVATE ex16_billing GTest::gmock_main)
gtest_discover_tests(ex16_unit_testing_tests
  TEST_PREFIX "${prefix}"
  DISCOVERY_MODE PRE_TEST
  PROPERTIES TIMEOUT 60)

add_test(NAME "${prefix}Grader.baseline"
  COMMAND ${CMAKE_COMMAND} "-DEXE=$<TARGET_FILE:ex16_unit_testing_tests>"
          -DMIN_TESTS=30 -P "${grader_script}")
set_tests_properties("${prefix}Grader.baseline" PROPERTIES TIMEOUT 60)

# The whole suite must finish within the budget: real retry backoff would
# sleep for seconds unless the suite injects a fake Sleeper.
add_test(NAME "${prefix}Grader.time_budget"
  COMMAND ${CMAKE_COMMAND} "-DEXE=$<TARGET_FILE:ex16_unit_testing_tests>"
          -DMIN_TESTS=30 -P "${grader_script}")
set_tests_properties("${prefix}Grader.time_budget" PROPERTIES TIMEOUT 10)

# ---- Mutation grading ---------------------------------------------------
# Each mutant replaces exactly one occurrence of a pattern in billing.cpp.
# The mutant executable must FAIL (WILL_FAIL), so a suite that passes it is
# reported as a failure of the Grader test.

file(READ "${billing_src}" billing_text)

# Sets out_var to the number of non-overlapping occurrences of pattern in text.
function(_ex16_count_matches text pattern out_var)
  set(count 0)
  set(rest "${text}")
  while(TRUE)
    string(FIND "${rest}" "${pattern}" pos)
    if(pos EQUAL -1)
      break()
    endif()
    math(EXPR count "${count} + 1")
    string(LENGTH "${pattern}" pattern_len)
    math(EXPR start "${pos} + ${pattern_len}")
    string(SUBSTRING "${rest}" ${start} -1 rest)
  endwhile()
  set(${out_var} ${count} PARENT_SCOPE)
endfunction()

function(_ex16_mutant name pattern replacement)
  _ex16_count_matches("${billing_text}" "${pattern}" occurrences)
  if(NOT occurrences EQUAL 1)
    message(FATAL_ERROR
      "mutant ${name}: pattern occurs ${occurrences} times in billing.cpp (need exactly 1)")
  endif()
  string(REPLACE "${pattern}" "${replacement}" mutated "${billing_text}")
  file(WRITE "${mutant_dir}/${name}.cpp" "${mutated}")

  set(exe "ex16_mutant_${name}")
  add_executable(${exe} $<TARGET_OBJECTS:ex16_student_suite> "${mutant_dir}/${name}.cpp")
  target_include_directories(${exe} PRIVATE "${exercise_dir}")
  target_link_libraries(${exe} PRIVATE GTest::gmock_main)

  set(test "${prefix}Grader.mutant_${name}")
  add_test(NAME "${test}" COMMAND ${exe})
  set_tests_properties("${test}" PROPERTIES WILL_FAIL TRUE TIMEOUT 60)
endfunction()

# Rounding
_ex16_mutant(round_truncate
  [[amount * bps + kBpsDenominator / 2)]]
  [[amount * bps)]])
_ex16_mutant(round_half_down
  [[amount * bps + kBpsDenominator / 2)]]
  [[amount * bps + kBpsDenominator / 2 - 1)]])

# Boundaries
_ex16_mutant(tier_1000_exclusive
  [[subtotal >= 100000) return 1000]]
  [[subtotal > 100000) return 1000]])
_ex16_mutant(tier_200_exclusive
  [[subtotal >= 10000) return 200]]
  [[subtotal > 10000) return 200]])

# Validation
_ex16_mutant(zero_price_rejected
  [[unit_price_ < 0]]
  [[unit_price_ <= 0]])
_ex16_mutant(zero_quantity_allowed
  [[quantity_ < 1]]
  [[quantity_ < 0]])
_ex16_mutant(zero_total_charged
  [[totals.total <= 0]]
  [[totals.total < 0]])
_ex16_mutant(zero_attempts_allowed
  [[policy_.max_attempts < 1]]
  [[policy_.max_attempts < 0]])
_ex16_mutant(zero_net_days_rejected
  [[if (net_days < 0)]]
  [[if (net_days < 1)]])

# Operation ordering and arithmetic
_ex16_mutant(tax_before_discount
  [[apply_bps(t.subtotal - t.discount, invoice.tax_bps())]]
  [[apply_bps(t.subtotal, invoice.tax_bps())]])
_ex16_mutant(due_date_off_by_one
  [[chr::days{net_days}]]
  [[chr::days{net_days + 1}]])

# Clock and timezone
_ex16_mutant(timezone_shift
  [[chr::floor<chr::days>(instant)]]
  [[chr::floor<chr::days>(instant + chr::hours{5})]])
_ex16_mutant(overdue_on_due_day
  [[return today > due;]]
  [[return today >= due;]])

# Retry policy and backoff math
_ex16_mutant(backoff_uncapped
  [[return std::min(delay, policy_.max_delay);]]
  [[return delay;]])
_ex16_mutant(backoff_first_doubled
  [[for (int i = 1; i < attempt]]
  [[for (int i = 0; i < attempt]])
_ex16_mutant(extra_attempt
  [[if (attempt >= policy_.max_attempts)]]
  [[if (attempt + 1 >= policy_.max_attempts)]])

# Retry classification
_ex16_mutant(declined_retried
  [[catch (const TransientGatewayError& err)]]
  [[catch (const BillingError& err)]])

# Logging
_ex16_mutant(transient_logged_as_info
  [[logger_.log(LogLevel::warning,]]
  [[logger_.log(LogLevel::info,]])
_ex16_mutant(declined_logged_as_warning
  [[LogLevel::error,
                  std::format("payment declined]]
  [[LogLevel::warning,
                  std::format("payment declined]])

# Gateway call arguments and post-charge side effects
_ex16_mutant(idempotency_key_swapped
  [[invoice.currency(), token, invoice.id()]]
  [[invoice.currency(), invoice.id(), token]])
_ex16_mutant(receipt_shows_subtotal
  [[format_money(totals.total), invoice.currency()]]
  [[format_money(totals.subtotal), invoice.currency()]])

# Error isolation
_ex16_mutant(receipt_failure_fails_charge
  [[catch (const std::exception& err)]]
  [[catch (const std::logic_error& err)]])

# File atomicity
_ex16_mutant(temp_file_left_behind
  [[fs::remove(temp, ignored);]]
  [[fs::exists(temp, ignored);]])
_ex16_mutant(previous_file_deleted
  [[fs::remove(temp, ignored);]]
  [[fs::remove(path, ignored); fs::remove(temp, ignored);]])
