#include "onUpload.hpp"
void Raptor::Common::Parsers::Handlers::onUpload(const Header& header, std::string_view payload) noexcept {
    header.print();
    std::println("payload: {}", payload);
}
