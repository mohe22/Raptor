#pragma once

#include "base.hpp"
#include "core/config.hpp"
#include "common/parser/tcp.hpp"
#include "libs/net/include/connection.hpp"
#include "libs/net/include/epoll.hpp"

namespace Raptor::Core::Session {

class TcpSession :
    public Net::Poll::Descriptor,
    public Base  {
        public:

        TcpSession(std::unique_ptr<Net::Connection> conn, uint64_t id)
        : Base(id, Common::Types::ServerType::TCP), connection_(std::move(conn)){
            fd = connection_->getSocket();
        }
        ~TcpSession() noexcept {
            std::println("~TcpSession id={}", id());
        }

        Net::Result<size_t> read(void* buf, size_t len) noexcept override {
            auto result = connection_->receive(buf, len);
            if (result) touch();
            return result;
        }
        Net::Result<size_t> write(const void* buf, size_t len) noexcept override{
            return connection_->send(buf, len);
        }

        Net::Result<std::pair<std::string_view, uint16_t>> getAddressStr() const noexcept override {
            auto ipRes = connection_->getIp();
            if (!ipRes) return std::unexpected(ipRes.error());

            auto portRes = connection_->getPort();
            if (!portRes) return std::unexpected(portRes.error());

            return std::make_pair(ipRes.value().data(), portRes.value());
        }




        Common::Parsers::TcpParser<uint8_t,Config::RECV_BUFFER_SIZE> parser;

    private:
        std::unique_ptr<Net::Connection> connection_;
    };

} // namespace Raptor::Core::Peer
