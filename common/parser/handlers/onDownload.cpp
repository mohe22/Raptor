#include "onDownload.hpp"

void Raptor::Common::Parsers::Handlers::onDownload(const Header& header, std::string_view payload) noexcept {
    header.print();
    std::println("payload: {}", payload);
}
