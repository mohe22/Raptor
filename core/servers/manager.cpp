#include "manager.hpp"
#include "core/api/utils.hpp"
#include "core/context.hpp"
#include "core/db/logRepository.hpp"
#include "core/servers/base.hpp"
#include "core/servers/tcp/tcp.hpp"
#include "core/servers/udp/udp.hpp"
#include <expected>
#include <print>

namespace Raptor::Core::Servers {

    Manager::~Manager() {
        stopAll();
    }

    void Manager::stopAll() noexcept {
        std::vector<std::unique_ptr<Base>> toStop;
        {
            std::unique_lock lock(mutex_);
            for (auto& [name, server] : servers_)
                server->stop();
            for (auto& [name, server] : servers_)
                toStop.push_back(std::move(server));
            servers_.clear();
        }
        for (auto& server : toStop)
            server->join();
    }

    const ServerConfig Manager::getServerConfig(const std::string& name) const noexcept {
        std::shared_lock lock(mutex_);
        if (!servers_.contains(name)) return {};
        return servers_.at(name)->config();
    }

    std::expected<void, std::string> Manager::createServer(Common::Types::ServerType type, ServerConfig config) {
        if (!ServerConfig::isValid(config)) {
            Context::get().logs().warn(
                Db::LogCategory::Server,
                "SERVER_INVALID_CONFIG",
                std::format("[{}] invalid server config", config.instanceName),
                "",
                config.instanceName
            );
            return std::unexpected{"Invalid server config."};
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
                return std::unexpected{"Unsupported server type yet!"};
        }

        if (!server->start()) {
            Context::get().logs().error(
                Db::LogCategory::Server,
                "SERVER_START_FAIL",
                std::format("[{}] failed to start: {}", config.instanceName, server->getErrorMsg()),
                "",
                config.instanceName
            );
            return std::unexpected{"Failed to start the server, check the logs."};
        }

        {
            std::unique_lock lock(mutex_);
            servers_.emplace(config.instanceName, std::move(server));
        }

        Context::get().logs().info(
            Db::LogCategory::Server,
            "SERVER_CREATED",
            std::format("[{}] {} server created", config.instanceName, Common::Types::ToString(type)),
            config.toJsonStr(),
            config.instanceName
        );
        return {};
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
        owned->join();
        return true;
    }

    bool Manager::pauseServer(const std::string& name) {
        std::unique_lock lock(mutex_);
        auto it = servers_.find(name);
        if (it == servers_.end()) return false;
        it->second->pause();
        return true;
    }

    bool Manager::resumeServer(const std::string& name) {
        std::unique_lock lock(mutex_);
        auto it = servers_.find(name);
        if (it == servers_.end()) return false;
        it->second->resume();
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

    bool Manager::hasServer(const std::string& name) const noexcept {
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

            pool.runningServerCount += running ? 1 : 0;
            pool.totalBytesReceived += rx;
            pool.totalBytesSent     += tx;

            const size_t sessionCount = server->sessionManager.count();
            pool.totalSessionCount  += sessionCount;
            pool.activeSessionCount += running ? sessionCount : 0;

            pool.servers.push_back(ServerEntry{
                .name    = config.instanceName,
                .ipAddress = config.ip,
                .port = config.port,
                .type = server->type(),
                .status= status,
                .sessionCount  = sessionCount,
                .bytesSent     = tx,
                .bytesReceived = rx,
                .startTime     = server->uptime()
            });
        }

        return pool;
    }

    std::expected<void, std::string> Manager::updateServer(const std::string& name,const uint16_t port,const std::string& ip){
        Common::Types::ServerType type;
        ServerConfig newConfig;

        {
            std::shared_lock lock(mutex_);

            auto it = servers_.find(name);
            if (it == servers_.end())
                return std::unexpected{"server not found!"};

            const ServerConfig& current = it->second->config();

            newConfig = current;
            newConfig.port = port;
            strncpy(newConfig.ip, ip.c_str(), sizeof(newConfig.ip) - 1);
            newConfig.ip[sizeof(newConfig.ip) - 1] = '\0';
            newConfig.ipType = Core::Api::Utils::detectIpType(ip);

            if (!ServerConfig::isValid(newConfig))
                return std::unexpected{"invalid config!"};

            type = it->second->type();
        }

        // Build and start the new server before touching the old one
        std::unique_ptr<Base> newServer;
        switch (type) {
            case Common::Types::ServerType::TCP:
                newServer = std::make_unique<TcpServer>(newConfig);
                break;
            case Common::Types::ServerType::UDP:
                newServer = std::make_unique<UdpServer>(newConfig);
                break;
            default:
                return std::unexpected{"Unsupported server type!"};
        }

        if (!newServer->start()) {
            Context::get().logs().error(
                Db::LogCategory::Server,
                "SERVER_UPDATED",
                std::format("[{}] Faild to update server {}",newServer->config().instanceName ,newServer->getErrorMsg()),
                newConfig.toJsonStr(),
                name
            );
            return std::unexpected{
                std::format("Failed to start updated server: {}", newServer->getErrorMsg())
            };
        }


        {
            std::unique_lock lock(mutex_);

            auto it = servers_.find(name);
            // if the server was removed while we starting the new server (edge case)
            if (it == servers_.end()) {
                newServer->stop();
                newServer->join();
                return std::unexpected{"server was removed during update!"};
            }
            // now the newServer holde the old server, which when eixt the function
            // it will get destroyed
            it->second.swap(newServer);
        }


        Context::get().logs().info(
            Db::LogCategory::Server,
            "SERVER_UPDATED",
            std::format("[{}] restarted with new config", name),
            newConfig.toJsonStr(),
            name
        );

        return {};
    }

    std::optional<ServerInfo> Manager::getServerInfo(const std::string& name) const noexcept {
        std::shared_lock lock(mutex_);
        auto it = servers_.find(name);
        if (it == servers_.end()) return std::nullopt;

        const auto& server = it->second;
        uint64_t rx = 0, tx = 0;
        server->getRxBytes(rx);
        server->getTxBytes(tx);

        return ServerInfo{
            .config   = server->config(),
            .status  = server->status(),
            .error = server->getErrorMsgStr(),
            .type  = server->type(),
            .sessionCounter = server->sessionManager.count(),
            .uptimeSeconds  = server->uptimeSeconds(),
            .rxBytes = rx,
            .txBytes = tx
        };
    }

} // namespace Raptor::Core::Servers
