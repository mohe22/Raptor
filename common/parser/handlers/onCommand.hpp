
#pragma once
#include "header.hpp"

namespace Raptor::Common::Parsers::Handlers {
    void onCommand(const Header& header, std::string_view payload) noexcept;
}
