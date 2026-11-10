#include "container_lint.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <set>
#include <functional>
#include <utility>

namespace cppref::deployment::container {
namespace {

struct Instruction {
    std::size_t line;
    std::string op;    // upper-cased keyword
    std::string args;  // remainder, trimmed
};

bool is_ws(char c) { return std::isspace(static_cast<unsigned char>(c)) != 0; }

std::string_view trim(std::string_view s) {
    while (!s.empty() && is_ws(s.front())) s.remove_prefix(1);
    while (!s.empty() && is_ws(s.back())) s.remove_suffix(1);
    return s;
}

std::vector<std::string_view> words(std::string_view s) {
    std::vector<std::string_view> out;
    s = trim(s);
    while (!s.empty()) {
        const auto end = s.find_first_of(" \t");
        out.push_back(s.substr(0, end));
        if (end == std::string_view::npos) break;
        s = trim(s.substr(end));
    }
    return out;
}

// Joins continuation lines; the instruction's line is its first physical line.
std::vector<Instruction> parse_dockerfile(std::string_view text) {
    std::vector<Instruction> out;
    std::string pending;
    std::size_t start = 0;
    bool continuing = false;

    auto flush = [&] {
        const auto logical = trim(pending);
        if (!logical.empty()) {
            const auto space = logical.find_first_of(" \t");
            std::string op(logical.substr(0, space));
            for (char& c : op) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            const auto args = space == std::string_view::npos ? std::string_view{} : trim(logical.substr(space));
            out.push_back(Instruction{start, std::move(op), std::string(args)});
        }
        pending.clear();
        continuing = false;
    };

    std::size_t line_no = 0;
    std::size_t pos = 0;
    while (pos < text.size()) {
        const auto nl = text.find('\n', pos);
        const auto raw = text.substr(pos, nl == std::string_view::npos ? std::string_view::npos : nl - pos);
        pos = nl == std::string_view::npos ? text.size() : nl + 1;
        ++line_no;

        const auto line = trim(raw);
        if (line.empty() || line.front() == '#') continue;
        if (!continuing) start = line_no;
        if (line.back() == '\\') {
            pending += std::string(line.substr(0, line.size() - 1));
            pending += ' ';
            continuing = true;
            continue;
        }
        pending += std::string(line);
        flush();
    }
    if (continuing) flush();
    return out;
}

// Image reference is pinned when it has a digest, or a non-latest tag containing a digit.
bool is_pinned(std::string_view image) {
    if (image.find("@sha256:") != std::string_view::npos) return true;
    const auto slash = image.rfind('/');
    const auto colon = image.rfind(':');
    if (colon == std::string_view::npos) return false;
    if (slash != std::string_view::npos && colon < slash) return false;
    const auto tag = image.substr(colon + 1);
    return tag != "latest" && tag.find_first_of("0123456789") != std::string_view::npos;
}

// Drops leading --flags (e.g. --interval=30s) and returns the remainder.
std::string_view skip_flags(std::string_view s) {
    s = trim(s);
    while (s.starts_with("--")) {
        const auto end = s.find_first_of(" \t");
        s = end == std::string_view::npos ? std::string_view{} : trim(s.substr(end));
    }
    return s;
}

bool is_numeric(std::string_view s) {
    return !s.empty() && std::ranges::all_of(s, [](char c) { return c >= '0' && c <= '9'; });
}

}  // namespace

std::vector<LintIssue> lint_dockerfile(std::string_view text) {
    const auto instructions = parse_dockerfile(text);
    std::vector<LintIssue> issues;
    auto add = [&](std::size_t line, std::string message) { issues.push_back({line, std::move(message)}); };

    std::size_t stages = 0;
    std::size_t last_from = 0;
    bool last_stage_copies_from = false;
    const Instruction* last_user = nullptr;
    const Instruction* last_entrypoint = nullptr;
    const Instruction* last_cmd = nullptr;
    const Instruction* last_health = nullptr;
    bool has_expose = false;

    for (const auto& in : instructions) {
        if (in.op == "FROM") {
            ++stages;
            last_from = in.line;
            last_stage_copies_from = false;
            std::string_view image;
            for (auto w : words(in.args)) {
                if (!w.starts_with("--")) {
                    image = w;
                    break;
                }
            }
            if (image != "scratch" && !is_pinned(image)) {
                add(in.line, "FROM '" + std::string(image) + "' must pin a version tag or digest");
            }
        } else if (in.op == "COPY" || in.op == "ADD") {
            const auto ws = words(in.args);
            if (std::ranges::any_of(ws, [](std::string_view w) { return w.starts_with("--from="); })) {
                last_stage_copies_from = true;
            }
            // The last word is the destination; everything before it is a source.
            for (std::size_t i = 0; i + 1 < ws.size(); ++i) {
                if (ws[i].starts_with("--")) continue;
                if (ws[i] == "." || ws[i] == "./") {
                    add(in.line, "COPY . copies the whole build context; copy explicit files");
                }
            }
        } else if (in.op == "USER") {
            last_user = &in;
        } else if (in.op == "EXPOSE") {
            has_expose = true;
            for (auto w : words(in.args)) {
                const auto port = w.substr(0, w.find('/'));
                if (!is_numeric(port)) {
                    add(in.line, "EXPOSE '" + std::string(w) + "' is not a numeric port");
                }
            }
        } else if (in.op == "ENTRYPOINT") {
            last_entrypoint = &in;
        } else if (in.op == "CMD") {
            last_cmd = &in;
        } else if (in.op == "HEALTHCHECK") {
            last_health = &in;
        }
    }

    if (stages < 2) {
        add(0, "expected a multi-stage build (builder plus slim runtime)");
    } else if (!last_stage_copies_from) {
        add(last_from, "final stage copies nothing from a build stage (COPY --from)");
    }

    if (!has_expose) add(0, "no EXPOSE instruction");

    if (!last_user) {
        add(0, "no USER instruction; container would run as root");
    } else {
        const auto who = words(last_user->args);
        const std::string_view name = who.empty() ? std::string_view{} : who.front();
        if (name == "root" || name == "0" || name.starts_with("root:") || name.starts_with("0:")) {
            add(last_user->line, "final USER must not be root");
        }
    }

    if (!last_entrypoint) {
        add(0, "no ENTRYPOINT instruction");
    } else if (!trim(last_entrypoint->args).starts_with('[')) {
        add(last_entrypoint->line, "ENTRYPOINT must use exec (JSON array) form");
    }

    if (!last_cmd) {
        add(0, "no CMD instruction");
    } else if (!trim(last_cmd->args).starts_with('[')) {
        add(last_cmd->line, "CMD must use exec (JSON array) form");
    }

    if (!last_health) {
        add(0, "no HEALTHCHECK instruction");
    } else {
        const auto rest = skip_flags(last_health->args);
        if (rest.starts_with("NONE")) {
            add(last_health->line, "HEALTHCHECK NONE disables the probe");
        } else if (!(rest.starts_with("CMD") && trim(rest.substr(3)).starts_with('['))) {
            add(last_health->line, "HEALTHCHECK CMD must use exec (JSON array) form");
        }
    }

    return issues;
}

std::vector<LintIssue> lint_dockerignore(std::string_view text) {
    constexpr std::array<std::string_view, 3> required{".git", "build", ".env"};
    std::vector<LintIssue> issues;
    std::set<std::string, std::less<>> excluded;

    auto normalize = [](std::string_view s) {
        while (s.starts_with("./")) s.remove_prefix(2);
        while (s.starts_with("**/")) s.remove_prefix(3);
        while (!s.empty() && s.back() == '/') s.remove_suffix(1);
        return s;
    };

    std::size_t line_no = 0;
    std::size_t pos = 0;
    while (pos < text.size()) {
        const auto nl = text.find('\n', pos);
        const auto raw = text.substr(pos, nl == std::string_view::npos ? std::string_view::npos : nl - pos);
        pos = nl == std::string_view::npos ? text.size() : nl + 1;
        ++line_no;

        auto line = trim(raw);
        if (line.empty() || line.front() == '#') continue;
        const bool negated = line.front() == '!';
        if (negated) line = trim(line.substr(1));

        const auto norm = normalize(line);
        for (auto r : required) {
            if (norm != r) continue;
            if (negated) {
                issues.push_back({line_no, "'" + std::string(norm) + "' is re-included with '!'"});
            } else {
                excluded.insert(std::string(norm));
            }
        }
    }

    for (auto r : required) {
        if (!excluded.contains(r)) {
            issues.push_back({0, "must exclude '" + std::string(r) + "'"});
        }
    }
    return issues;
}

}  // namespace cppref::deployment::container
