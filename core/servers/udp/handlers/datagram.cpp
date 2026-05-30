#include "datagram.hpp"
#include <print>
#include <string_view>


namespace Raptor::Core::Servers::Udp::Handlers{
    size_t handleDatagram(const uint8_t*buf,const std::size_t recvBytes,const Session::UdpSession* peer,const Net::Servers::Udp& server){
        std::println("{} -({})",std::string_view(reinterpret_cast<const char*>(buf),recvBytes),recvBytes);

        server.sendTo(buf, recvBytes, peer->getAddress());

        // auto span = std::span<const char>(
            // reinterpret_cast<const char*>(buf),
            // recvBytes
        // );

        // bool isMore = peer->handler.parse(span);
        // if(!isMore) return;
        // std::println("got full packet");

        // peer->sendFile("/home/mohe/Documents/C++/Raptor/agent/build/f.txt","f",Task::Priority::High);

        // auto r = peer->flushSendQueue();
        // if(!r){
            // std::println("{}",Net::toErrorString(r.error()));
        // }
        // return r.value();
        return  0;
    };
}
