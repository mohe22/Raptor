

#pragma once
#include "header.hpp"

namespace Raptor::Common::Parsers::Handlers {
    void onUpload(const Header& header, std::string_view payload) noexcept;
}
