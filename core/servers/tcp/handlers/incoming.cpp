#include "incoming.hpp"
#include <print>
namespace Raptor::Core::Servers::Tcp::Handlers {
   Net::Result<std::unique_ptr<Net::Connection>>  handleIncoming(Net::Servers::Tcp* server){
        Net::Result<std::unique_ptr<Net::Connection>> conn = server->accept();
        if (!conn) {
            return conn;
        }
        std::println("Connection accepted!");
        return conn;
    }
}
