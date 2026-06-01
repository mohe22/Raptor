#pragma once

#include "base.hpp"
#include "core/config.hpp"
#include "common/parser/tcp.hpp"
#include "header.hpp"
#include "libs/net/include/connection.hpp"
#include "libs/net/include/epoll.hpp"
#include "register.hpp"

namespace Raptor::Core::Session {

    inline void printRegister(const Raptor::Common::Register& reg) {
        std::println("Hostname          : {}", reg.hostname);
        std::println("Username          : {}", reg.username);
        std::println("Home Dir          : {}", reg.homeDir);
        std::println("Shell             : {}", reg.shell);
        std::println("Is Admin          : {}", reg.isAdmin);
        std::println("Is Sudoer         : {}", reg.isSudoer);
        std::println("Domain            : {}", reg.domain);
        std::println("OS                : {} {}", reg.os, reg.osVersion);
        std::println("Kernel            : {}", reg.kernelVersion);
        std::println("Arch              : {}", reg.arch);
        std::println("CPU               : {}", reg.cpu);
        std::println("CPU Cores         : {}", reg.cpuCores);
        std::println("RAM               : {} bytes", reg.ramBytes);
        std::println("Disk Total        : {} bytes", reg.diskTotalBytes);
        std::println("Disk Free         : {} bytes", reg.diskFreeBytes);
        std::println("Uptime (sec)      : {}", reg.uptimeSeconds);
        std::println("PID               : {}", reg.pid);
        std::println("Process Path      : {}", reg.processPath);
        std::println("Process Name      : {}", reg.processName);
        std::println("Is Docker         : {}", reg.isDocker);
        std::println("Is VM             : {} ({})", reg.isVM, reg.vmType);
        std::println("Internal IP       : {}", reg.internalIp);
        std::println("Internal IP2      : {}", reg.internalIp2);
        std::println("MAC Address       : {}", reg.macAddress);
        std::println("Default Gateway   : {}", reg.defaultGateway);
        std::println("DNS Server        : {}", reg.dnsServer);
        std::println("Is Proxy          : {}", reg.isProxy);
        std::println("Proxy URL         : {}", reg.proxyUrl);
        std::println("Domain Joined     : {}", reg.isDomainJoined);
        std::println("SELinux Enabled   : {}", reg.selinuxEnabled);
        std::println("AppArmor Enabled  : {}", reg.apparmorEnabled);
        std::println("Timezone          : {}", reg.timezone);
        std::println("Locale            : {}", reg.locale);
        std::println("First Seen        : {}", reg.firstSeenAt);
    }
    class TcpSession :
    public Net::Poll::Descriptor,
    public Base,
    public Common::Parsers::TcpParser<uint8_t,Config::RECV_BUFFER_SIZE>
    {
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

        std::string getAddressStr() const noexcept override {
            auto ipRes = connection_->getIp();
            if (!ipRes) return "";

            auto portRes = connection_->getPort();
            if (!portRes) return "";

            return std::format("{}:{}", ipRes.value().data(), portRes.value());
        }

        void onCommand(const Common::Header& h, std::string_view body) noexcept override{};
        void onUpload(const Common::Header& h, std::string_view body) noexcept override {};
        void onDownload(const Common::Header& h, std::string_view body) noexcept override {};

        void onRegister(const Common::Header& h, std::string_view body) noexcept override {
            auto result = Common::Register::deserialize(
                std::span<const uint8_t>(
                    reinterpret_cast<const uint8_t*>(body.data()),
                    body.size()
                )
            );

            if (!result) {
                std::println("[onRegister] deserialize failed: {}", result.error());
                return;
            }

            h.print();
            printRegister(*result);

        }
        private:
        std::unique_ptr<Net::Connection> connection_;
    };

} // namespace Raptor::Core::Peer
