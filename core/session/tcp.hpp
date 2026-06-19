#pragma once

#include "base.hpp"
#include "core/config.hpp"
#include "common/parser/tcp.hpp"
#include "core/session/queue.hpp"
#include "core/session/task/task.hpp"
#include "header.hpp"
#include "libs/net/include/connection.hpp"
#include "libs/net/include/epoll.hpp"

#include "core/api/controllers/socket.hpp"
#include <print>
#include <string>


namespace Raptor::Core::Session {

    class TcpSession :
    public Net::Poll::Descriptor,
    public Base,
    public Common::Parsers::TcpParser<uint8_t,Config::RECV_BUFFER_SIZE>
    {
        public:
        TcpSession(std::unique_ptr<Net::Connection> conn, uint64_t id,const std::string&serverId)
        :
        Base(id, Common::Types::ServerType::TCP,serverId),
        connection_(std::move(conn)){
            fd = connection_->getSocket();
        }
        ~TcpSession() noexcept {
            // in case client disconnected report to ui
            Api::WebSocket::dispatchSessionDisconnected(std::to_string(id()),connectedTo());
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

        void onCommand(const Common::Header&, std::string_view command) noexcept override{
            // header.print();
            std::println("{}",command);
        };
        void onUpload(const Common::Header&, std::string_view) noexcept override {};
        void onDownload(const Common::Header&, std::string_view) noexcept override {};
        void onRegister(const Common::Header&, std::string_view body) noexcept override {
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

            setRegistrationInfo(*result);

            // dispatch to UI
            Api::WebSocket::dispatchSessionConnected(
                result->username,
                result->hostname,
                result->timezone,
                std::to_string(id()),
                connectedAtStr(),
                result->os,
                connectedTo(),
                getAddressStr()
            );
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
                // move the task to unorder_map id: task

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


                                sentThisCall += static_cast<size_t>(res.value());
                                task.offset  += static_cast<size_t>(res.value());

                                if (task.offset < Common::Header::SIZE)
                                    return sentThisCall;
                            }

                            task.state  = Tasks::TaskState::SendingData;
                            task.offset = 0;
                            [[fallthrough]];
                        }
                        case Tasks::TaskState::SendingData: {
                            size_t remaining = cmd.cmd.size() - task.offset;

                            if (remaining > 0) {
                                auto res = connection_->send(
                                    cmd.cmd.data() + task.offset,
                                    remaining
                                );
                                if (!res) return res;

                                sentThisCall += static_cast<size_t>(res.value());
                                task.offset  += static_cast<size_t>(res.value());
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
