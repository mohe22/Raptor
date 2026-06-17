#include "shutdown.hpp"
#include <print>
namespace Raptor::Core::Servers::Tcp::Handlers {
    void handleShutdown(Core::Session::TcpSession* ){
        // auto [ip,port] = conn->getRemoteIpPort();
        // std::println("[TCP-shutdown] {}:{} was disconnected!", ip,port);
        // Core::Api::WebSocketChat::sendLeftPeerEvent(conn->getId());
    };
}
