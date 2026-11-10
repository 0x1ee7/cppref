#pragma once

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace cppref::raii_and_memory {

class Document;

// One section of a document: a "# heading" line and the lines below it.
class Section {
public:
    std::string heading;  // text after "# "; empty for preamble text
    std::string body;     // following lines joined with '\n', no trailing newline

    // The owning document, or nullptr once it has been destroyed. Never keeps the document alive.
    std::shared_ptr<const Document> document() const;

    // Engineer-owned: a std::weak_ptr<const Document> back-reference, set by Document::parse.
};

// Immutable parsed document. Only parse() creates documents, so every document is
// shared-owned and sections can link back to it.
class Document {
public:
    // Lines starting with "# " open a section. Text before the first heading becomes a section
    // with an empty heading, but only when it contains non-blank text.
    static std::shared_ptr<const Document> parse(std::string_view text);

    const std::vector<std::shared_ptr<const Section>>& sections() const;

private:
    Document();
    // Engineer-owned: std::vector<std::shared_ptr<const Section>> sections_;
};

// Shares ownership of *section but points at its body (aliasing constructor).
// Returns nullptr when section is nullptr.
std::shared_ptr<const std::string> body_of(const std::shared_ptr<const Section>& section);

using DocumentParser = std::function<std::shared_ptr<const Document>(std::string_view)>;

// Cache keyed by string. Holds only weak references: callers decide document lifetime.
class DocumentCache {
public:
    explicit DocumentCache(DocumentParser parser = &Document::parse);

    // Returns the cached document for key while it is alive. Otherwise parses text,
    // caches a weak reference to the result, and returns it.
    std::shared_ptr<const Document> get_or_parse(const std::string& key, std::string_view text);

    // Number of entries whose document is still alive.
    std::size_t live_count() const;
    // Removes entries whose document is gone; returns how many were removed.
    std::size_t purge_expired();
    // Number of entries, including expired ones not yet purged.
    std::size_t entry_count() const noexcept { return 0; }

    // Engineer-owned: DocumentParser parser_; and a std::unordered_map<std::string, std::weak_ptr<const Document>>.
};

}  // namespace cppref::raii_and_memory
