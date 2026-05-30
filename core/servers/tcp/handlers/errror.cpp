#include "error.hpp"
namespace Raptor::Core::Servers::Tcp::Handlers {
    void handleError(Core::Session::TcpSession* , const Net::Error& error){
        std::println("[TCP] error={}" , Net::toErrorString(error));
    }
}
