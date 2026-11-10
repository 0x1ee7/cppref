#include "downloader.hpp"

#include <stdexcept>

namespace cppref::concurrency {

BoundedDownloader::BoundedDownloader(Fetcher fetch, std::size_t max_concurrent)
    : fetch_(std::move(fetch)), slots_(static_cast<std::ptrdiff_t>(max_concurrent)) {
    throw std::logic_error("not implemented: BoundedDownloader::BoundedDownloader");
}

std::string BoundedDownloader::download(std::string_view url) {
    (void)url;
    throw std::logic_error("not implemented: BoundedDownloader::download");
}

std::vector<std::string> BoundedDownloader::download_all(std::span<const std::string> urls) {
    (void)urls;
    throw std::logic_error("not implemented: BoundedDownloader::download_all");
}

std::size_t BoundedDownloader::in_flight() const {
    throw std::logic_error("not implemented: BoundedDownloader::in_flight");
}

std::size_t BoundedDownloader::peak_concurrency() const {
    throw std::logic_error("not implemented: BoundedDownloader::peak_concurrency");
}

}  // namespace cppref::concurrency
