// Wall-clock timestamps shared by the manifest, report and snapshot code.
#pragma once

#include <chrono>

namespace cppref::files {

// UTC instant with microsecond precision: std::chrono::sys_time is
// system_clock::time_point, i.e. measured from the Unix epoch.
// File times (std::filesystem::file_time_type) live on file_clock and must be
// converted with std::chrono::clock_cast before they are stored here.
using Timestamp = std::chrono::sys_time<std::chrono::microseconds>;

}  // namespace cppref::files
