#include "onRegister.hpp"

namespace Raptor::Common::Parsers::Handlers {
    void onRegister(const Header& header, std::string_view payload) noexcept {

        header.print();
        std::println("Register payload: {}", payload);
    }
}
