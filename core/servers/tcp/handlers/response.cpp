#include "response.hpp"

namespace Raptor::Core::Servers::Tcp::Handlers {

    Net::Result<std::size_t> handleResponse(Session::TcpSession* peer){
       // auto result = peer->flushSendQueue();
        // if(!result) {
            // std::println("error:{}",Net::toErrorString(result.error()));
            // return std::unexpected(result.error());
        // }

        // return result.value();
        return 0;
    }
}
