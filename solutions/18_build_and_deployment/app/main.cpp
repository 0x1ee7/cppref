// Process entry point: wires the real environment and stdio into cli::run.
// Engineer-owned glue, identical in both trees.
#include "cli.hpp"

#include <cstdlib>
#include <exception>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#ifdef _MSC_VER
#pragma warning(disable : 4996)  // std::getenv is the portable lookup; MSVC flags it as unsafe
#endif

int main(int argc, char** argv) {
    using namespace cppref::deployment;

    std::vector<std::string> storage(argv + 1, argv + argc);
    std::vector<std::string_view> args(storage.begin(), storage.end());

    const EnvLookup env = [](std::string_view name) -> std::optional<std::string> {
        const std::string key(name);
        if (const char* value = std::getenv(key.c_str())) {
            return std::string(value);
        }
        return std::nullopt;
    };

    try {
        return cli::run(args, env, std::cout, std::cerr);
    } catch (const std::exception& e) {
        std::cerr << "fatal: " << e.what() << '\n';
        return 70;
    }
}
