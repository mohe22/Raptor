
#pragma once

#include "base.hpp"
#include "libs/net/include/address.hpp"
namespace Raptor::Core::Session {
class UdpSession : public Base {
    public:
       UdpSession(Net::Address address, uint64_t id)
        : Base(id, Common::Types::ServerType::UDP)
        , address_(std::move(address)){}

        Net::Result<std::pair<std::string_view, uint16_t>> getAddressStr() const noexcept override {
            auto ipRes = address_.getIp();
            if (!ipRes) return std::unexpected(ipRes.error());

            auto portRes = address_.getPort();
            if (!portRes) return std::unexpected(portRes.error());

            return std::make_pair(ipRes.value().data(), portRes.value());
        }
        Net::Address getAddress() const noexcept {
            return address_;
        }



    private:
        Net::Address address_;
    };

} // namespace Raptor::Core::Peer
