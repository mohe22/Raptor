#pragma once

#include "core/session/udp.hpp"
#include "libs/net/include/server.hpp"
namespace Raptor::Core::Servers::Udp::Handlers{
    size_t handleDatagram(const uint8_t*,const std::size_t,const Session::UdpSession*,const Net::Servers::Udp&);

};
