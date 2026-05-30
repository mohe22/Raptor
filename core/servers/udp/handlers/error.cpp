

#include "error.hpp"
#include <print>
namespace Raptor::Core::Servers::Udp::Handlers{
    void handleError(const Net::Error& error){
        std::println("Error {} ",Net::toErrorString(error));
    }
};
