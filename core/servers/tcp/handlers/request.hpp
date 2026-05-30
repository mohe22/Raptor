#pragma once

#include "core/session/tcp.hpp"

namespace Raptor::Core::Servers::Tcp::Handlers {
    Net::Result<std::size_t> handleRequest(Core::Session::TcpSession* session);
}
