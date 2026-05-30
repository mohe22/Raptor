#pragma once
#include "libs/net/include/server.hpp"

namespace Raptor::Core::Servers::Tcp::Handlers {
     Net::Result<std::unique_ptr<Net::Connection>> handleIncoming(Net::Servers::Tcp* server);
}
