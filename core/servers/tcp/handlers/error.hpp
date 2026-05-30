/*
    * @file Raptor/core/listeners/tcp/handlers/error.hpp
    * @brief this file handle the error callbacks for the TCP listener.
*/
#pragma once
#include "core/session/tcp.hpp"
namespace Raptor::Core::Servers::Tcp::Handlers {
    void handleError(Core::Session::TcpSession* peer, const Net::Error& error);
}
