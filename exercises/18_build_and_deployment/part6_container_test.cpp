#include "container_lint.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace dep = cppref::deployment::container;
namespace fs = std::filesystem;

namespace {

std::string read_text(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream s;
    s << in.rdbuf();
    return s.str();
}

std::string describe(const std::vector<dep::LintIssue>& issues) {
    std::string out;
    for (const auto& i : issues) out += "line " + std::to_string(i.line) + ": " + i.message + "\n";
    return out;
}

bool has_issue(const std::vector<dep::LintIssue>& issues, std::size_t line, std::string_view fragment) {
    return std::ranges::any_of(issues, [&](const dep::LintIssue& i) {
        return i.line == line && i.message.find(fragment) != std::string::npos;
    });
}

// Line numbers used below:
//  1 FROM gcc  2 WORKDIR  3 COPY  4 RUN  5 FROM debian  6 COPY --from  7 USER
//  8 EXPOSE    9 HEALTHCHECK  10 ENTRYPOINT  11 CMD
const std::string kBase = R"DF(FROM gcc:14.2.0-bookworm AS build
WORKDIR /src
COPY app/main.cpp app/main.cpp
RUN g++ -std=c++23 -o /out/svc app/main.cpp
FROM debian:12.7-slim
COPY --from=build /out/svc /usr/local/bin/svc
USER 10001:10001
EXPOSE 8080
HEALTHCHECK --interval=30s CMD ["/usr/local/bin/svc", "check-config"]
ENTRYPOINT ["/usr/local/bin/svc"]
CMD ["serve"]
)DF";

std::string replaced(std::string text, std::string_view from, std::string_view to) {
    const auto at = text.find(from);
    EXPECT_NE(at, std::string::npos) << "fixture drift: " << from;
    if (at != std::string::npos) text.replace(at, from.size(), to);
    return text;
}

}  // namespace

TEST(Part6Container, ShippedDockerfileLintsClean) {
    const auto issues = dep::lint_dockerfile(read_text(fs::path(CPPREF_WORKDIR) / "Dockerfile"));
    EXPECT_TRUE(issues.empty()) << describe(issues);
}

TEST(Part6Container, ShippedDockerignoreLintsClean) {
    const auto issues = dep::lint_dockerignore(read_text(fs::path(CPPREF_WORKDIR) / ".dockerignore"));
    EXPECT_TRUE(issues.empty()) << describe(issues);
}

TEST(Part6Container, BaseFixtureIsClean) {
    const auto issues = dep::lint_dockerfile(kBase);
    EXPECT_TRUE(issues.empty()) << describe(issues);
}

TEST(Part6Container, EmptyDockerfileFails) {
    EXPECT_FALSE(dep::lint_dockerfile("").empty());
}

TEST(Part6Container, UnpinnedImagesAreReported) {
    auto issues = dep::lint_dockerfile(replaced(kBase, "FROM debian:12.7-slim", "FROM debian:latest"));
    EXPECT_TRUE(has_issue(issues, 5, "pin")) << describe(issues);

    issues = dep::lint_dockerfile(replaced(kBase, "FROM gcc:14.2.0-bookworm AS build", "FROM gcc AS build"));
    EXPECT_TRUE(has_issue(issues, 1, "pin")) << describe(issues);
}

TEST(Part6Container, CopyingWholeContextIsReported) {
    const auto issues = dep::lint_dockerfile(replaced(kBase, "COPY app/main.cpp app/main.cpp", "COPY . /src"));
    EXPECT_TRUE(has_issue(issues, 3, "COPY .")) << describe(issues);
}

TEST(Part6Container, RootUserIsReported) {
    EXPECT_TRUE(has_issue(dep::lint_dockerfile(replaced(kBase, "USER 10001:10001", "USER 0:0")), 7, "root"));
    EXPECT_TRUE(has_issue(dep::lint_dockerfile(replaced(kBase, "USER 10001:10001", "USER root")), 7, "root"));
}

TEST(Part6Container, MissingUserIsReported) {
    const auto issues = dep::lint_dockerfile(replaced(kBase, "USER 10001:10001\n", ""));
    EXPECT_TRUE(has_issue(issues, 0, "USER")) << describe(issues);
}

TEST(Part6Container, NonNumericExposeIsReported) {
    const auto issues = dep::lint_dockerfile(replaced(kBase, "EXPOSE 8080", "EXPOSE http"));
    EXPECT_TRUE(has_issue(issues, 8, "EXPOSE")) << describe(issues);
}

TEST(Part6Container, ExecFormIsRequiredForCmdEntrypointAndHealthcheck) {
    EXPECT_TRUE(has_issue(
        dep::lint_dockerfile(replaced(kBase, R"(CMD ["/usr/local/bin/svc", "check-config"])",
                                      "CMD /usr/local/bin/svc check-config")),
        9, "exec"));
    EXPECT_TRUE(has_issue(
        dep::lint_dockerfile(replaced(kBase, R"(ENTRYPOINT ["/usr/local/bin/svc"])",
                                      "ENTRYPOINT /usr/local/bin/svc")),
        10, "exec"));
    EXPECT_TRUE(has_issue(
        dep::lint_dockerfile(replaced(kBase, R"(CMD ["serve"])", "CMD serve")), 11, "exec"));
}

TEST(Part6Container, HealthcheckNoneIsReported) {
    const auto issues = dep::lint_dockerfile(
        replaced(kBase, R"(--interval=30s CMD ["/usr/local/bin/svc", "check-config"])", "NONE"));
    EXPECT_TRUE(has_issue(issues, 9, "NONE")) << describe(issues);
}

TEST(Part6Container, SingleStageIsReported) {
    const std::string text =
        "FROM gcc:14.2.0-bookworm\nUSER 10001\nEXPOSE 8080\n"
        "HEALTHCHECK CMD [\"/x\"]\nENTRYPOINT [\"/x\"]\nCMD [\"serve\"]\n";
    EXPECT_TRUE(has_issue(dep::lint_dockerfile(text), 0, "multi-stage"));
}

TEST(Part6Container, RuntimeStageMustCopyFromBuild) {
    const auto issues = dep::lint_dockerfile(
        replaced(kBase, "COPY --from=build /out/svc /usr/local/bin/svc", "COPY /out/svc /usr/local/bin/svc"));
    EXPECT_TRUE(has_issue(issues, 5, "COPY --from")) << describe(issues);
}

TEST(Part6Container, CommentsAndContinuationsAreHandled) {
    EXPECT_TRUE(dep::lint_dockerfile(replaced(kBase, "WORKDIR /src", "# COPY . /src is forbidden\nWORKDIR \\\n  /src"))
                    .empty());
    const auto issues = dep::lint_dockerfile(replaced(kBase, "COPY app/main.cpp app/main.cpp", "COPY \\\n  . /src"));
    EXPECT_TRUE(has_issue(issues, 3, "COPY .")) << describe(issues);
}

TEST(Part6Container, DockerignoreMissingEntriesAreReported) {
    const auto issues = dep::lint_dockerignore(".env\nbuild\n");
    EXPECT_TRUE(has_issue(issues, 0, ".git")) << describe(issues);
}

TEST(Part6Container, DockerignoreAcceptsEquivalentSpellings) {
    const auto issues = dep::lint_dockerignore(".git\n**/.git/\nbuild/\n./build\n**/.env\n");
    EXPECT_TRUE(issues.empty()) << describe(issues);
}

TEST(Part6Container, DockerignoreReincludeIsReported) {
    const auto issues = dep::lint_dockerignore(".git\nbuild\n.env\n!.env\n");
    EXPECT_TRUE(has_issue(issues, 4, ".env")) << describe(issues);
}

TEST(Part6Container, DockerignoreCommentDoesNotCount) {
    const auto issues = dep::lint_dockerignore("# .env\n.git\nbuild\n");
    EXPECT_TRUE(has_issue(issues, 0, ".env")) << describe(issues);
}
