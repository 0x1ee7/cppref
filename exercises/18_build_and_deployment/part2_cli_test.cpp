#include "cli.hpp"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <span>
#include <sstream>
#include <string>
#include <vector>

#ifndef _WIN32
#include <sys/wait.h>
#endif

namespace dep = cppref::deployment;
namespace fs = std::filesystem;

namespace {

dep::EnvLookup env_from(std::map<std::string, std::string> vars) {
    return [vars = std::move(vars)](std::string_view name) -> std::optional<std::string> {
        auto it = vars.find(std::string(name));
        if (it == vars.end()) return std::nullopt;
        return it->second;
    };
}

std::map<std::string, std::string> valid_vars() {
    return {{"DATABASE_URL", "sqlite:/tmp/orders.db"}, {"API_TOKEN", "0123456789abcdef-secret"}};
}

struct Outcome {
    int code;
    std::string out;
    std::string err;
};

Outcome run_cli(const std::vector<std::string_view>& args, std::map<std::string, std::string> vars) {
    std::ostringstream out;
    std::ostringstream err;
    const int code = dep::cli::run(std::span<const std::string_view>(args), env_from(std::move(vars)), out, err);
    return {code, out.str(), err.str()};
}

// Writes content to a unique file under the temp directory; removed on destruction.
class TempFile {
public:
    explicit TempFile(std::string_view content)
        : path_(fs::temp_directory_path() /
                ("cppref-ex18-cli-" + std::to_string(counter_.fetch_add(1)) + "-" +
                 std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".env")) {
        std::ofstream out(path_, std::ios::binary);
        out << content;
    }
    ~TempFile() {
        std::error_code ec;
        fs::remove(path_, ec);
    }
    TempFile(const TempFile&) = delete;
    TempFile& operator=(const TempFile&) = delete;

    [[nodiscard]] std::string str() const { return path_.string(); }

private:
    static inline std::atomic<unsigned> counter_{0};
    fs::path path_;
};

std::string read_text(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream s;
    s << in.rdbuf();
    return s.str();
}

#ifndef _WIN32
int exit_status(int raw) { return WIFEXITED(raw) ? WEXITSTATUS(raw) : -1; }
#endif

// Runs the built CLI binary with its output captured in `sink`. Returns the exit code.
int run_binary(const std::string& arguments, const TempFile& sink) {
    const std::string command = "\"" + std::string(CPPREF_CLI_PATH) + "\" " + arguments + " > \"" +
                                sink.str() + "\" 2>&1";
#ifdef _WIN32
    return std::system(("\"" + command + "\"").c_str());
#else
    return exit_status(std::system(command.c_str()));
#endif
}

}  // namespace

TEST(Part2Cli, VersionPrintsVersionLineWithoutConfig) {
    const auto r = run_cli({"version"}, {});
    EXPECT_EQ(r.code, 0);
    EXPECT_TRUE(r.out.starts_with("cppref-service ")) << r.out;
    EXPECT_TRUE(r.out.ends_with(")\n")) << r.out;
    EXPECT_TRUE(r.err.empty()) << r.err;
}

TEST(Part2Cli, UsageErrorsExitTwo) {
    const std::vector<std::vector<std::string_view>> cases{
        {}, {"frobnicate"}, {"--bogus", "serve"}, {"--env-file"}, {"version", "extra"}};
    for (const auto& args : cases) {
        const auto r = run_cli(args, valid_vars());
        EXPECT_EQ(r.code, 2) << (args.empty() ? std::string("<none>") : std::string(args.front()));
        EXPECT_TRUE(r.err.starts_with("error: ")) << r.err;
    }
}

TEST(Part2Cli, CheckConfigSucceedsWithValidEnvironment) {
    const auto r = run_cli({"check-config"}, valid_vars());
    EXPECT_EQ(r.code, 0);
    EXPECT_EQ(r.out, "configuration ok\n");
    EXPECT_TRUE(r.err.empty()) << r.err;
}

TEST(Part2Cli, CheckConfigPrintsEveryProblemToStderr) {
    const auto r = run_cli({"check-config"}, {});
    EXPECT_EQ(r.code, 2);
    EXPECT_TRUE(r.out.empty()) << r.out;
    EXPECT_EQ(r.err, "error: DATABASE_URL: required but not set\nerror: API_TOKEN: required but not set\n");
}

TEST(Part2Cli, DryRunReportsDefaultListenAddress) {
    const auto r = run_cli({"serve", "--dry-run"}, valid_vars());
    EXPECT_EQ(r.code, 0);
    EXPECT_EQ(r.out, "dry run: cppref-service would listen on 127.0.0.1:8080\n");
}

TEST(Part2Cli, FlagsOverrideHostAndPort) {
    const auto r = run_cli({"serve", "--dry-run", "--host", "0.0.0.0", "--port", "9000"}, valid_vars());
    EXPECT_EQ(r.code, 0);
    EXPECT_EQ(r.out, "dry run: cppref-service would listen on 0.0.0.0:9000\n");
}

TEST(Part2Cli, PortFromEnvironmentIsUsedWhenNoFlag) {
    auto vars = valid_vars();
    vars["PORT"] = "7000";
    const auto r = run_cli({"serve", "--dry-run"}, vars);
    EXPECT_EQ(r.code, 0);
    EXPECT_EQ(r.out, "dry run: cppref-service would listen on 127.0.0.1:7000\n");
}

TEST(Part2Cli, ServeRejectsBadPortAndUnknownOptions) {
    for (std::string_view port : {"0", "70000", "abc"}) {
        const auto r = run_cli({"serve", "--dry-run", "--port", port}, valid_vars());
        EXPECT_EQ(r.code, 2) << port;
        EXPECT_TRUE(r.err.starts_with("error: --port must be")) << r.err;
    }
    const auto unknown = run_cli({"serve", "--bogus"}, valid_vars());
    EXPECT_EQ(unknown.code, 2);
    EXPECT_NE(unknown.err.find("unknown serve option '--bogus'"), std::string::npos) << unknown.err;
}

TEST(Part2Cli, ServeWithInvalidConfigExitsTwo) {
    const auto r = run_cli({"serve", "--dry-run"}, {});
    EXPECT_EQ(r.code, 2);
    EXPECT_NE(r.err.find("error: DATABASE_URL: required but not set"), std::string::npos) << r.err;
}

TEST(Part2Cli, EnvFileSuppliesConfiguration) {
    const TempFile file("API_TOKEN=0123456789abcdef-file\n");
    const auto path = file.str();
    const auto r = run_cli({"--env-file", path, "check-config"}, {{"DATABASE_URL", "sqlite:/tmp/x.db"}});
    EXPECT_EQ(r.code, 0) << r.err;
    EXPECT_EQ(r.out, "configuration ok\n");
}

TEST(Part2Cli, UnreadableEnvFileExitsTwo) {
    const auto missing = (fs::temp_directory_path() / "cppref-ex18-does-not-exist.env").string();
    const auto r = run_cli({"--env-file", missing, "check-config"}, valid_vars());
    EXPECT_EQ(r.code, 2);
    EXPECT_EQ(r.err.rfind("error: cannot read ", 0), 0u) << r.err;
}

TEST(Part2Cli, MalformedEnvFileNamesLine) {
    const TempFile file("OK=1\nbad line\n");
    const auto path = file.str();
    const auto r = run_cli({"--env-file", path, "check-config"}, valid_vars());
    EXPECT_EQ(r.code, 2);
    EXPECT_EQ(r.err.rfind("error: line 2: ", 0), 0u) << r.err;
}

TEST(Part2Cli, BinaryPrintsVersion) {
    const TempFile sink("");
    EXPECT_EQ(run_binary("version", sink), 0);
    EXPECT_TRUE(read_text(sink.str()).starts_with("cppref-service ")) << read_text(sink.str());
}

TEST(Part2Cli, BinaryUsageErrorExitsTwo) {
    const TempFile sink("");
    EXPECT_EQ(run_binary("bogus", sink), 2);
    EXPECT_NE(read_text(sink.str()).find("unknown command 'bogus'"), std::string::npos);
}
