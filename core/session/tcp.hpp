#pragma once

#include "base.hpp"
#include "core/config.hpp"
#include "common/parser/tcp.hpp"
#include "core/session/queue.hpp"
#include "core/session/task/task.hpp"
#include "header.hpp"
#include "libs/net/include/connection.hpp"
#include "libs/net/include/epoll.hpp"


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
    }
    class TcpSession :
    public Net::Poll::Descriptor,
    public Base,
    public Common::Parsers::TcpParser<uint8_t,Config::RECV_BUFFER_SIZE>
    {
        public:
        TcpSession(std::unique_ptr<Net::Connection> conn, uint64_t id,const std::string& connectedTo)
        : Base(id, Common::Types::ServerType::TCP,connectedTo), connection_(std::move(conn)){
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
            auto ipRes = connection_->getAddress().getIp();
            if (!ipRes) return "";

            auto portRes = connection_->getAddress().getPort();
            if (!portRes) return "";

            return std::format("{}:{}", ipRes.value().data(), portRes.value());
        }

        void onCommand(const Common::Header&, std::string_view) noexcept override{};
        void onUpload(const Common::Header&, std::string_view) noexcept override {};
        void onDownload(const Common::Header&, std::string_view) noexcept override {};
        void onRegister(const Common::Header&h, std::string_view body) noexcept override {
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
            setRegistrationInfo(*result);


        }


        void sendCommand(std::string cmd, const Common::PacketId& id) {
            sendQ_.push(Tasks::makeCommand(std::move(cmd), id));
        }

        Net::Result<size_t> flushSendQueue() {
            size_t totalSent = 0;

            while (true) {
                // 1. get the front task and exit if queue is empty
                Tasks::Task* task = sendQ_.tryFront();
                if (!task)
                    return totalSent;

                // 2. try to send as much of the task as the socket will accept
                Net::Result<size_t> res = dispatch(*task);
                if (!res) {
                    // 3. if socket full return what we sent so far, or propagate the error
                    if (res.error() == Net::Error::WouldBlock)
                        return totalSent > 0 ? Net::Result<size_t>{totalSent} : res;
                    return res;
                }

                totalSent += res.value();

                // 4. task fully sent remove it and try the next one
                if (task->state == Tasks::TaskState::Done) {
                    sendQ_.tryPop();
                    continue;
                }

                // 5. task partially sent stop and wait for the next writable event
                return totalSent;
            }
        }
        Net::Result<size_t> dispatch(Tasks::Task& task) {
            return std::visit(Tasks::Overload {
                [&](Tasks::Command& cmd) -> Net::Result<size_t> {
                    size_t sentThisCall = 0;

                    switch (task.state) {

                        case Tasks::TaskState::SendingHeader: {
                            size_t remaining = task.headerBuffer.size() - task.offset;

                            if (remaining > 0) {
                                auto res = connection_->send(
                                    task.headerBuffer.data() + task.offset,
                                    remaining
                                );
                                if (!res) return res;

                                sentThisCall += res.value();
                                task.offset += res.value();

                                if (task.offset < Common::Header::SIZE)
                                    return sentThisCall;
                            }

                            task.state  = Tasks::TaskState::SendingData;
                            task.offset = 0;
                        }

                        case Tasks::TaskState::SendingData: {
                            size_t remaining = cmd.cmd.size() - task.offset;

                            if (remaining > 0) {
                                auto res = connection_->send(
                                    cmd.cmd.data() + task.offset,
                                    remaining
                                );
                                if (!res) return res;

                                sentThisCall += res.value();
                                task.offset  += res.value();
                            }

                            if (task.offset == cmd.cmd.size())
                                task.state = Tasks::TaskState::Done;

                            return sentThisCall;
                        }

                        case Tasks::TaskState::Done:
                            return sentThisCall;
                    }

                    return sentThisCall;
                }
            }, task.payload);
        }
        private:
        std::unique_ptr<Net::Connection> connection_;
    };

} // namespace Raptor::Core::Peer
