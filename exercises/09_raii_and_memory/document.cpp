#include "document.hpp"

#include <stdexcept>

namespace cppref::raii_and_memory {

std::shared_ptr<const Document> Section::document() const {
    throw std::logic_error("not implemented: Section::document");
}

Document::Document() {}

std::shared_ptr<const Document> Document::parse(std::string_view text) {
    (void)text;
    throw std::logic_error("not implemented: Document::parse");
}

const std::vector<std::shared_ptr<const Section>>& Document::sections() const {
    throw std::logic_error("not implemented: Document::sections");
}

std::shared_ptr<const std::string> body_of(const std::shared_ptr<const Section>& section) {
    (void)section;
    throw std::logic_error("not implemented: body_of");
}

DocumentCache::DocumentCache(DocumentParser parser) {
    (void)parser;
    throw std::logic_error("not implemented: DocumentCache::DocumentCache");
}

std::shared_ptr<const Document> DocumentCache::get_or_parse(const std::string& key, std::string_view text) {
    (void)key;
    (void)text;
    throw std::logic_error("not implemented: DocumentCache::get_or_parse");
}

std::size_t DocumentCache::live_count() const {
    throw std::logic_error("not implemented: DocumentCache::live_count");
}

std::size_t DocumentCache::purge_expired() {
    throw std::logic_error("not implemented: DocumentCache::purge_expired");
}

}  // namespace cppref::raii_and_memory
