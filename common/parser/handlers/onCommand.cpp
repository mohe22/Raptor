#include "onCommand.hpp"

void Raptor::Common::Parsers::Handlers::onCommand(const Header& header, std::string_view payload) noexcept {
    header.print();
    std::println("payload: {}", payload);
}
