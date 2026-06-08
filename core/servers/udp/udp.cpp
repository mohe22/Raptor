
#include "udp.hpp"
#include "core/session/udp.hpp"

namespace Raptor::Core::Servers {


    bool UdpServer::init() noexcept {
        const auto& config = this->config();

        auto fail = [&](int err, const char* msg) {
            error.set(err, msg);
            return false;
        };

        if (auto r = server_.init(config.ipType); !r)
            return fail(errno, Net::toErrorString(r.error()).data());

        if (auto r = server_.setNonBlocking(); !r)
            return fail(errno, Net::toErrorString(r.error()).data());

        if (auto r = server_.setReusePort(); !r)
            return fail(errno, Net::toErrorString(r.error()).data());

        if (auto r = server_.setReuseAddress(); !r)
            return fail(errno, Net::toErrorString(r.error()).data());

        if (auto r = server_.bind(config.ip, config.port); !r)
            return fail(errno, Net::toErrorString(r.error()).data());
        if (auto r = epoll_.init(config.epollTimeout); !r)
            return fail(errno, Net::toErrorString(r.error()).data());
        error.clear();
        return true;
    }



    bool UdpServer::run() noexcept{

        Net::Poll::Descriptor serverfd{server_.getSocket()};
        if (auto r = epoll_.add(&serverfd); !r) {
            error.set(errno, Net::toErrorString(r.error()).data());
            Net::platformClose(serverfd.fd);
            return false;
        }

           epoll_.onRead([&](Net::Poll::Descriptor*) {
            uint8_t bufTmp[65536];
            while (true) {
                auto result = server_.receiveFrom(bufTmp, sizeof(bufTmp));
                if (!result) {
                    if (result.error() != Net::Error::WouldBlock)
                        callbacks_.error(result.error());
                    break;
                }
                auto& [recvBytes, sender] = result.value();
                if (recvBytes > 0)
                    addRxBytes(recvBytes);
                Session::UdpSession* peer = sessionManager.getOrCreate<Session::UdpSession>(sender,config().instanceName);
                size_t replySize = callbacks_.datagram(bufTmp, recvBytes, peer, server_);
                if (replySize > 0)
                    addTxBytes(replySize);
            }
        });

        epoll_.onError<Net::Poll::Descriptor>([&](Net::Poll::Descriptor*, const Net::Error& err) {
            callbacks_.error(err);
        });

        epoll_.onTimeout([&] {
            callbacks_.timeout();
        });


        if (auto r = epoll_.watch(); !r) {
            error.set(errno, Net::toErrorString(r.error()).data());
            return false;
        }



        return true;
    }

} // namespace Raptor::Core::Listeners::Udp
