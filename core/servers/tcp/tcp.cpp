#include "tcp.hpp"

namespace Raptor::Core::Servers {

    bool TcpServer::init() noexcept {
        const auto& config = this->config();

        auto fail = [&](int err, const char* msg) {
            error.set(err, msg);
            return false;
        };

        if (auto r = server_.init(config.ipType);    !r) return fail(errno, Net::toErrorString(r.error()).data());
        if (auto r = server_.setNonBlocking();        !r) return fail(errno, Net::toErrorString(r.error()).data());
        if (auto r = server_.setReusePort();          !r) return fail(errno, Net::toErrorString(r.error()).data());
        if (auto r = server_.setReuseAddress();       !r) return fail(errno, Net::toErrorString(r.error()).data());
        if (auto r = server_.bind(config.ip, config.port); !r) return fail(errno, Net::toErrorString(r.error()).data());
        if (auto r = server_.listen();                !r) return fail(errno, Net::toErrorString(r.error()).data());
        if (auto r = epoll_.init(config.epollTimeout);!r) return fail(errno, Net::toErrorString(r.error()).data());

        error.clear();
        return true;
    }

    void TcpServer::destroyClient(Session::TcpSession* client) noexcept {
        if (!client) return;
        callbacks_.shutdown(client);
        if (!sessionManager.remove(client->id()))
            std::println("[DESTROY] WARNING — session {} not found in manager", client->id());
    }

    void TcpServer::closeClient(Session::TcpSession* client, const Net::Error& err) noexcept {
        callbacks_.error(client, err);
        epoll_.closeDescriptor(client);
    }

    bool TcpServer::rearmClient(Session::TcpSession* client) noexcept {
        if (auto r = epoll_.rearm(client); !r) {
            std::println("[REARM] failed for session {}: {}", client->id(), Net::toErrorString(r.error()));
            closeClient(client, r.error());
            return false;
        }
        return true;
    }

    void TcpServer::onAccept() noexcept {
        auto conn = callbacks_.incoming(&server_);
        if (!conn) return;

        if (auto r = conn.value()->setNonBlocking(); !r) {
            std::println("[ACCEPT] setNonBlocking failed: {}", Net::toErrorString(r.error()));
            return;
        }

        auto session = std::make_unique<Session::TcpSession>(std::move(conn.value()), Common::nextId());
        Session::TcpSession* client = sessionManager.add(std::move(session));
        if (!client) {
            std::println("[ACCEPT] session limit reached — connection rejected");
            return;
        }

        if (auto r = epoll_.add(client); !r) {
            std::println("[ACCEPT] epoll add failed: {}", Net::toErrorString(r.error()));
            sessionManager.remove(client->id());
            return;
        }

        std::println("[ACCEPT] session {} connected from {}", client->id(), client->getAddressStr());

    }

    void TcpServer::onRead(Session::TcpSession* client) noexcept {
        auto r = callbacks_.request(client);
        if (!r) {
            if (r.error() == Net::Error::WouldBlock) {
                rearmClient(client);
                return;
            }
            closeClient(client, r.error());
            return;
        }
        addRxBytes(r.value());
        rearmClient(client);
    }

    void TcpServer::onWrite(Session::TcpSession* client) noexcept {
        Net::Result<size_t> r{0};

        while (true) {
            r = callbacks_.response(client);
            if (!r || r.value() == 0) break;
        }

        if (!r) {
            if (r.error() == Net::Error::WouldBlock) {
                rearmClient(client); // buffer full, retry on next EPOLLOUT
                return;
            }
            closeClient(client, r.error());
            return;
        }
        addTxBytes(r.value());
        rearmClient(client);
    }

    void TcpServer::onTimeout() noexcept {
        sessionManager.forEachOf<Session::TcpSession>([&](Session::TcpSession* client) {
            rearmClient(client);
        });
        callbacks_.timeout();
    }

    bool TcpServer::run() noexcept {
        Net::Poll::Descriptor serverfd{server_.getSocket()};

        if (auto r = epoll_.add(&serverfd); !r) {
            error.set(errno, Net::toErrorString(r.error()).data());
            Net::platformClose(serverfd.fd);
            return false;
        }

        epoll_.onRead([&](Net::Poll::Descriptor* desc) {
            if (desc->fd == server_.getSocket()) {
                onAccept();
                return;
            }
            onRead(static_cast<Session::TcpSession*>(desc));
        });

        epoll_.onWrite([&](Session::TcpSession* client) {
            onWrite(client);
        });

        epoll_.onError<Session::TcpSession>([&](Session::TcpSession* client, const Net::Error& err) {
            std::println("[ERROR] session {} — {}", client->id(), Net::toErrorString(err));
            closeClient(client, err);
        });

        epoll_.onClose<Session::TcpSession>([&](Session::TcpSession* client) {
            destroyClient(client);
        });

        epoll_.onTimeout([&] {
            onTimeout();
        });

        if (auto r = epoll_.watch(); !r) {
            error.set(errno, Net::toErrorString(r.error()).data());
            return false;
        }

        return true;
    }

} // namespace Raptor::Core::Servers
