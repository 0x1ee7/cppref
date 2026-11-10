#include "cli.hpp"

#include "build_info.hpp"
#include "shutdown.hpp"

#include <charconv>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <expected>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <system_error>
#include <thread>

namespace cppref::deployment::cli {
namespace {

constexpr int kOk = 0;
constexpr int kUsage = 2;
constexpr std::string_view kUsageLine =
    "usage: cppref-service [--env-file PATH] {serve [--host H] [--port P] [--dry-run] | check-config | version}";

int usage_error(std::ostream& err, std::string_view message) {
    err << "error: " << message << '\n' << kUsageLine << '\n';
    return kUsage;
}

std::expected<std::string, std::string> read_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return std::unexpected("cannot read " + path);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

struct ServeOptions {
    std::string host = "127.0.0.1";
    std::optional<std::uint16_t> port;
    bool dry_run = false;
};

std::expected<std::uint16_t, std::string> parse_port(std::string_view text) {
    unsigned long value = 0;
    const auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (ec != std::errc{} || ptr != text.data() + text.size() || value < 1 || value > 65535) {
        return std::unexpected("--port must be an integer 1-65535, got '" + std::string(text) + "'");
    }
    return static_cast<std::uint16_t>(value);
}

std::expected<ServeOptions, std::string> parse_serve(std::span<const std::string_view> args) {
    ServeOptions opts;
    for (std::size_t i = 0; i < args.size(); ++i) {
        const auto arg = args[i];
        if (arg == "--dry-run") {
            opts.dry_run = true;
            continue;
        }
        if (arg == "--host" || arg == "--port") {
            if (i + 1 >= args.size()) return std::unexpected(std::string(arg) + " needs a value");
            const auto value = args[++i];
            if (arg == "--host") {
                opts.host = std::string(value);
            } else {
                auto port = parse_port(value);
                if (!port) return std::unexpected(port.error());
                opts.port = *port;
            }
            continue;
        }
        return std::unexpected("unknown serve option '" + std::string(arg) + "'");
    }
    return opts;
}

void print_config_errors(std::ostream& err, const ConfigError& e) {
    for (const auto& entry : e.errors()) err << "error: " << entry << '\n';
}

}  // namespace

int run(std::span<const std::string_view> args, const EnvLookup& env, std::ostream& out, std::ostream& err) {
    std::size_t i = 0;
    std::optional<std::string> env_file;
    while (i < args.size() && args[i].starts_with("--")) {
        if (args[i] != "--env-file") return usage_error(err, "unknown option '" + std::string(args[i]) + "'");
        if (i + 1 >= args.size()) return usage_error(err, "--env-file needs a PATH");
        env_file = std::string(args[i + 1]);
        i += 2;
    }
    if (i >= args.size()) return usage_error(err, "missing command");

    const auto command = args[i];
    const auto rest = args.subspan(i + 1);

    if (command == "version") {
        if (!rest.empty()) return usage_error(err, "version takes no arguments");
        out << build_info().version_line() << '\n';
        return kOk;
    }
    if (command != "serve" && command != "check-config") {
        return usage_error(err, "unknown command '" + std::string(command) + "'");
    }

    DotenvMap dotenv;
    if (env_file) {
        auto text = read_file(*env_file);
        if (!text) {
            err << "error: " << text.error() << '\n';
            return kUsage;
        }
        auto parsed = load_dotenv(*text);
        if (!parsed) {
            err << "error: " << parsed.error() << '\n';
            return kUsage;
        }
        dotenv = std::move(*parsed);
    }
    const EnvLookup lookup = with_dotenv(env, std::move(dotenv));

    if (command == "check-config") {
        if (!rest.empty()) return usage_error(err, "check-config takes no arguments");
        try {
            (void)Config::from_env(lookup);
        } catch (const ConfigError& e) {
            print_config_errors(err, e);
            return kUsage;
        }
        out << "configuration ok\n";
        return kOk;
    }

    auto opts = parse_serve(rest);
    if (!opts) return usage_error(err, opts.error());

    Config config;
    try {
        config = Config::from_env(lookup);
    } catch (const ConfigError& e) {
        print_config_errors(err, e);
        return kUsage;
    }

    const std::uint16_t port = opts->port.value_or(config.port);
    if (opts->dry_run) {
        out << "dry run: " << config.app_name << " would listen on " << opts->host << ':' << port << '\n';
        return kOk;
    }

    // No HTTP listener in this exercise: stand in with a process that stays up until signalled.
    SignalGuard guard{SIGINT, SIGTERM};
    out << config.app_name << " ready on " << opts->host << ':' << port
        << " (no HTTP listener in this exercise; Ctrl+C to stop)\n" << std::flush;
    const auto token = guard.token();
    while (!token.stop_requested()) std::this_thread::sleep_for(std::chrono::milliseconds(10));
    return kOk;
}

}  // namespace cppref::deployment::cli
