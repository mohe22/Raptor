#include "response.hpp"

namespace Raptor::Core::Servers::Tcp::Handlers {

    Net::Result<std::size_t> handleResponse(Session::TcpSession* peer){
        return peer->flushSendQueue();
    }
}
