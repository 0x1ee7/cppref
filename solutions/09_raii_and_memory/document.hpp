#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace cppref::raii_and_memory {

class Document;

// One section. Holds a weak back-reference: sections never keep their document alive,
// which avoids the document <-> section ownership cycle.
class Section {
public:
    std::string heading;
    std::string body;

    std::shared_ptr<const Document> document() const { return owner_.lock(); }

private:
    friend class Document;
    std::weak_ptr<const Document> owner_;
};

// Immutable document; only parse() creates one, so it is always shared-owned.
class Document {
public:
    static std::shared_ptr<const Document> parse(std::string_view text);

    const std::vector<std::shared_ptr<const Section>>& sections() const noexcept { return sections_; }

private:
    Document() = default;
    std::vector<std::shared_ptr<const Section>> sections_;
};

// Aliasing constructor: shares section's control block, points at its body. Null in, null out.
std::shared_ptr<const std::string> body_of(const std::shared_ptr<const Section>& section);

using DocumentParser = std::function<std::shared_ptr<const Document>(std::string_view)>;

// Holds only weak references: the caller's shared_ptrs decide document lifetime.
class DocumentCache {
public:
    explicit DocumentCache(DocumentParser parser = &Document::parse);

    std::shared_ptr<const Document> get_or_parse(const std::string& key, std::string_view text);

    std::size_t live_count() const;
    std::size_t purge_expired();
    std::size_t entry_count() const noexcept { return entries_.size(); }

private:
    DocumentParser parser_;
    std::unordered_map<std::string, std::weak_ptr<const Document>> entries_;
};

}  // namespace cppref::raii_and_memory
