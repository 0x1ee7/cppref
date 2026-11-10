#include "document.hpp"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace cppref::raii_and_memory {

namespace {

bool is_blank(std::string_view s) {
    return s.find_first_not_of(" \t\r") == std::string_view::npos;
}

struct PendingSection {
    std::string heading;
    bool has_heading = false;
    std::vector<std::string_view> lines;
};

std::string join(const std::vector<std::string_view>& lines) {
    std::string out;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (i != 0) out += '\n';
        out += lines[i];
    }
    return out;
}

}  // namespace

std::shared_ptr<const Document> Document::parse(std::string_view text) {
    std::vector<PendingSection> pending(1);  // preamble: no heading
    std::size_t pos = 0;
    while (pos < text.size()) {
        const std::size_t nl = text.find('\n', pos);
        const std::size_t end = (nl == std::string_view::npos) ? text.size() : nl;
        const std::string_view line = text.substr(pos, end - pos);
        pos = (nl == std::string_view::npos) ? text.size() : nl + 1;
        if (line.starts_with("# ")) {
            pending.push_back(PendingSection{std::string(line.substr(2)), true, {}});
        } else {
            pending.back().lines.push_back(line);
        }
    }

    auto doc = std::shared_ptr<Document>(new Document());
    for (const PendingSection& p : pending) {
        const bool has_text = std::any_of(p.lines.begin(), p.lines.end(),
                                          [](std::string_view l) { return !is_blank(l); });
        if (!p.has_heading && !has_text) continue;

        auto sec = std::make_shared<Section>();
        sec->heading = p.heading;
        sec->body = join(p.lines);
        sec->owner_ = doc;
        doc->sections_.push_back(std::move(sec));
    }
    return doc;
}

std::shared_ptr<const std::string> body_of(const std::shared_ptr<const Section>& section) {
    if (!section) return nullptr;
    return std::shared_ptr<const std::string>(section, &section->body);
}

DocumentCache::DocumentCache(DocumentParser parser) : parser_(std::move(parser)) {}

std::shared_ptr<const Document> DocumentCache::get_or_parse(const std::string& key, std::string_view text) {
    if (auto it = entries_.find(key); it != entries_.end()) {
        if (auto live = it->second.lock()) return live;
    }
    std::shared_ptr<const Document> doc = parser_(text);
    entries_.insert_or_assign(key, doc);
    return doc;
}

std::size_t DocumentCache::live_count() const {
    return static_cast<std::size_t>(std::count_if(entries_.begin(), entries_.end(),
                                                  [](const auto& kv) { return !kv.second.expired(); }));
}

std::size_t DocumentCache::purge_expired() {
    return std::erase_if(entries_, [](const auto& kv) { return kv.second.expired(); });
}

}  // namespace cppref::raii_and_memory
