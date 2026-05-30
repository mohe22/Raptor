#pragma once
#include "core/session/tcp.hpp"
namespace Raptor::Core::Servers::Tcp::Handlers {
    void handleShutdown(Core::Session::TcpSession*);
}
