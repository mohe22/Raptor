#include "incoming.hpp"
namespace Raptor::Core::Servers::Tcp::Handlers {
   Net::Result<std::unique_ptr<Net::Connection>>  handleIncoming(Net::Servers::Tcp* server){
        Net::Result<std::unique_ptr<Net::Connection>> conn = server->accept();
        if (!conn) {
            return conn;
        }
        return conn;

    }
}
