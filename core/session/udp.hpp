
#pragma once

#include "base.hpp"
#include "header.hpp"
#include "libs/net/include/address.hpp"
namespace Raptor::Core::Session {
    class UdpSession :
    public Base {
        public:
        UdpSession(Net::Address address, uint64_t id,const std::string& connected)
        : Base(id, Common::Types::ServerType::UDP,connected)
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


        // void onCommand(const Common::Header& , std::string_view body) noexcept override{};
        // void onUpload(const Common::Header& , std::string_view body) noexcept override {};
        // void onDownload(const Common::Header& , std::string_view body) noexcept override {};

        // void onRegister(const Common::Header& h, std::string_view body) noexcept override {

        //     auto result = Common::Register::deserialize(
        //         std::span<const uint8_t>(
        //             reinterpret_cast<const uint8_t*>(body.data()),
        //             body.size()
        //         )
        //     );

        //     if (!result) {
        //         std::println("[onRegister] deserialize failed: {}", result.error());
        //         return;
        //     }


        // }
        private:
        Net::Address address_;
    };

} // namespace Raptor::Core::Peer
