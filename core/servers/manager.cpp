#include "manager.hpp"
#include <format>
#include "core/context.hpp"
#include "core/db/logRepository.hpp"
#include "core/servers/tcp/tcp.hpp"
#include "core/servers/udp/udp.hpp"

namespace Raptor::Core::Servers {

    Manager::~Manager() {
        stopAll();
    }

    const ServerConfig Manager::getServerConfig(const std::string& name) const noexcept {
        std::shared_lock lock(mutex_);
        if (!servers_.contains(name)) return {};
        return servers_.at(name)->config();
    }

    bool Manager::createServer(Common::Types::ServerType type, ServerConfig config) {
        if (!ServerConfig::isValid(config)) {
            Context::get().logs().warn(
                Db::LogCategory::Server,
                "SERVER_INVALID_CONFIG",
                std::format("[{}] invalid server config", config.instanceName)
            );
            return false;
        }

        std::unique_ptr<Base> server;
        switch (type) {
            case Common::Types::ServerType::TCP:
                server = std::make_unique<TcpServer>(config);
                break;
            case Common::Types::ServerType::UDP:
                server = std::make_unique<UdpServer>(config);
                break;
            default:
                return false;
        }

        if (!server->start()) {
            Context::get().logs().error(
                Db::LogCategory::Server,
                "SERVER_START_FAIL",
                std::format("[{}] Failed to start: {}", config.instanceName, server->getErrorMsg())
            );
            return false;
        }

        {
            std::unique_lock lock(mutex_);
            if (servers_.contains(config.instanceName)) return false;
            servers_.emplace(config.instanceName, std::move(server));
        }

        Context::get().logs().info(
            Db::LogCategory::Server,
            "SERVER_CREATED",
            std::format("[{}] {} server created", config.instanceName, Common::Types::ToString(type)),
            config.toJsonStr()
        );

        return true;
    }

    bool Manager::stopServer(const std::string& name) {
        std::unique_ptr<Base> owned;
        {
            std::unique_lock lock(mutex_);
            auto it = servers_.find(name);
            if (it == servers_.end()) return false;
            owned = std::move(it->second);
            servers_.erase(it);
        }

        owned->stop();

        Context::get().logs().info(
            Db::LogCategory::Server,
            "SERVER_STOP",
            std::format("[{}] server stopped", name)
        );

        return true;
    }

    bool Manager::pauseServer(const std::string& name) {
        {
            std::unique_lock lock(mutex_);
            auto it = servers_.find(name);
            if (it == servers_.end()) return false;
            it->second->pause();
        }

        Context::get().logs().info(
            Db::LogCategory::Server,
            "SERVER_PAUSE",
            std::format("[{}] server paused", name)
        );

        return true;
    }

    bool Manager::resumeServer(const std::string& name) {
        {
            std::unique_lock lock(mutex_);
            auto it = servers_.find(name);
            if (it == servers_.end()) return false;
            it->second->resume();
        }

        Context::get().logs().info(
            Db::LogCategory::Server,
            "SERVER_RESUME",
            std::format("[{}] server resumed", name)
        );

        return true;
    }

    bool Manager::isServerRunning(const std::string& name) const noexcept {
        std::shared_lock lock(mutex_);
        auto it = servers_.find(name);
        return it != servers_.end() && it->second->isRunning();
    }

    bool Manager::hasError(const std::string& name) const noexcept {
        std::shared_lock lock(mutex_);
        auto it = servers_.find(name);
        return it != servers_.end() && it->second->hasError();
    }

    ServerStatus Manager::getServerStatus(const std::string& name) const noexcept {
        std::shared_lock lock(mutex_);
        auto it = servers_.find(name);
        return it != servers_.end() ? it->second->status() : ServerStatus::Stopped;
    }

    void Manager::getServerMetrics(const std::string& name, uint64_t& rxBytes, uint64_t& txBytes) const noexcept {
        std::shared_lock lock(mutex_);
        auto it = servers_.find(name);
        if (it == servers_.end()) return;
        it->second->getRxBytes(rxBytes);
        it->second->getTxBytes(txBytes);
    }

    void Manager::stopAll() noexcept {
        std::unique_lock lock(mutex_);
        for (auto& [name, server] : servers_)
            server->stop();
        servers_.clear();
    }

    void Manager::joinAll() noexcept {
        std::shared_lock lock(mutex_);
        for (auto& [name, server] : servers_)
            server->join();
    }

    std::size_t Manager::count() const noexcept {
        std::shared_lock lock(mutex_);
        return servers_.size();
    }

    ServersInfoList Manager::getServersInfo() const noexcept {
        std::shared_lock lock(mutex_);
        ServersInfoList info;
        info.reserve(servers_.size());
        for (const auto& [name, server] : servers_) {
            uint64_t rx = 0, tx = 0;
            server->getTxBytes(tx);
            server->getRxBytes(rx);
            info.emplace_back(
                server->config(),
                server->status(),
                server->getErrorMsgStr(),
                server->type(),
                server->sessionManager.count(),
                server->uptimeSeconds(),
                rx,
                tx
            );
        }
        return info;
    }

    bool Manager::hasServer(const std::string& name) const noexcept{
        std::shared_lock lock(mutex_);
        return servers_.find(name) != servers_.end();
    }

    Server::SessionManager* Manager::getSessionManager(const std::string& name) const noexcept {
        std::shared_lock lock(mutex_);
        auto it = servers_.find(name);
        if (it == servers_.end()) return nullptr;
        return &it->second->sessionManager;
    }

    ServerPoolStatus Manager::getServerPoolStatus() const noexcept {
        std::shared_lock lock(mutex_);

        ServerPoolStatus pool{};
        pool.totalServerCount = servers_.size();
        pool.servers.reserve(servers_.size());

        for (const auto& [name, server] : servers_) {
            if (!server) continue;

            const auto& config = server->config();
            const auto  status = server->status();

            uint64_t rx{}, tx{};
            server->getRxBytes(rx);
            server->getTxBytes(tx);

            const bool running = (status == ServerStatus::Running);

            pool.runningServerCount  += running ? 1 : 0;
            pool.totalBytesReceived  += rx;
            pool.totalBytesSent += tx;

            const size_t sessionCount = server->sessionManager.count();
            pool.totalSessionCount   += sessionCount;
            pool.activeSessionCount  += running ? sessionCount : 0;

            pool.servers.push_back(ServerEntry{
                .name = config.instanceName,
                .ipAddress = config.ip,
                .port = config.port,
                .type = server->type(),
                .status = status,
                .sessionCount = sessionCount,
                .bytesSent = tx,
                .bytesReceived = rx,
                .startTime = server->uptime()
            });
        }

        return pool;
    }

} // namespace Raptor::Core::Servers
