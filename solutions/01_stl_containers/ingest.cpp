#include "ingest.hpp"

#include <cmath>
#include <cstddef>
#include <string_view>
#include <vector>

namespace cppref::stl {

std::size_t drop_invalid(std::vector<Reading>& readings) {
    const std::size_t before = readings.size();
    std::erase_if(readings, [](const Reading& r) { return std::isnan(r.value) || r.ts_ms < 0; });
    return before - readings.size();
}

std::vector<Reading> select_sensor(std::span<const Reading> readings, std::string_view sensor) {
    std::size_t matches = 0;
    for (const Reading& r : readings) {
        if (r.sensor == sensor) ++matches;
    }

    std::vector<Reading> out;
    out.reserve(matches);
    for (const Reading& r : readings) {
        if (r.sensor == sensor) out.push_back(r);
    }
    return out;
}

std::vector<std::string_view> split_fields(std::string_view line, char sep) {
    std::vector<std::string_view> out;
    std::size_t start = 0;
    while (true) {
        const std::size_t pos = line.find(sep, start);
        if (pos == std::string_view::npos) {
            out.push_back(line.substr(start));
            return out;
        }
        out.push_back(line.substr(start, pos - start));
        start = pos + 1;
    }
}

std::optional<std::string_view> hottest_sensor(std::span<const Reading> readings) {
    std::optional<std::size_t> best;
    for (std::size_t i = 0; i < readings.size(); ++i) {
        if (std::isnan(readings[i].value)) continue;
        if (!best || readings[i].value > readings[*best].value) best = i;
    }
    if (!best) return std::nullopt;
    return std::string_view{readings[*best].sensor};
}

}  // namespace cppref::stl
