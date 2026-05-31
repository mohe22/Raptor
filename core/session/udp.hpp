
#pragma once

#include "base.hpp"
#include "libs/net/include/address.hpp"
namespace Raptor::Core::Session {
class UdpSession : public Base {
    public:
       UdpSession(Net::Address address, uint64_t id)
        : Base(id, Common::Types::ServerType::UDP)
        , address_(std::move(address)){}

        std::string getAddressStr() const noexcept override {
            auto ipRes = address_.getIp();
            if (!ipRes) return "";

            auto portRes = address_.getPort();
            if (!portRes) return "";

            return std::format("{}:{}", ipRes.value().data(), portRes.value());
        }
        Net::Address getAddress() const noexcept {
            return address_;
        }



    private:
        Net::Address address_;
    };

} // namespace Raptor::Core::Peer
