#include "manager.hpp"
#include "core/api/utils.hpp"
#include "core/context.hpp"
#include "core/db/logRepository.hpp"
#include "core/servers/base.hpp"
#include "core/servers/tcp/tcp.hpp"
#include "core/servers/udp/udp.hpp"
#include <expected>

namespace Raptor::Core::Servers {

    Manager::~Manager() {
        stopAll();
    }

    const ServerConfig Manager::getServerConfig(const std::string& name) const noexcept {
        std::shared_lock lock(mutex_);
        if (!servers_.contains(name)) return {};
        return servers_.at(name)->config();
    }

    std::expected<void,std::string> Manager::createServer(Common::Types::ServerType type, ServerConfig config) noexcept {
        if (!ServerConfig::isValid(config)) {
            Context::get().logs().warn(
                Db::LogCategory::Server,
                "SERVER_INVALID_CONFIG",
                std::format("[{}] invalid server config", config.instanceName)
            );
            return  std::unexpected{"Invalid server config."};
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
                return  std::unexpected{"Unsupported server type yet!"};

        }

        if (!server->start()) {
            Context::get().logs().error(
                Db::LogCategory::Server,
                "SERVER_START_FAIL",
                std::format("[{}] Failed to start: {}", config.instanceName, server->getErrorMsg())
            );
            return std::unexpected{"Faild to start the server check the logs."};

        }

        {
            std::unique_lock lock(mutex_);
            if (servers_.contains(config.instanceName)) return std::unexpected{"the server name already exist!"};
            servers_.emplace(config.instanceName, std::move(server));
        }

        Context::get().logs().info(
            Db::LogCategory::Server,
            "SERVER_CREATED",
            std::format("[{}] {} server created", config.instanceName, Common::Types::ToString(type)),
            config.toJsonStr()
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
    std::expected<void,std::string> Manager::updateServer(const std::string& oldName, const uint16_t port,const std::string& ip, const std::string& newName) noexcept{
        Common::Types::ServerType type;
        ServerConfig newConfig;
        bool nameOnly = false;

        {
            std::unique_lock lock(mutex_);

            auto it = servers_.find(oldName);
            if (it == servers_.end())
                return std::unexpected{"server not found!"};

            if (newName != oldName && servers_.contains(newName))
                return std::unexpected{std::format(" {} already exist!",newName)};

            const ServerConfig& current = it->second->config();
            nameOnly = (current.port == port) &&
                       (strncmp(current.ip, ip.c_str(), sizeof(current.ip)) == 0);

            if (nameOnly) {
                std::unordered_map<std::string, std::unique_ptr<Base>>::node_type node = servers_.extract(it);
                node.mapped()->updateInstanceName(newName);
                node.key() = newName;
                servers_.insert(std::move(node));
            } else {
                type      = it->second->type();
                newConfig = current;

                newConfig.instanceName = newName;
                newConfig.port= port;
                strncpy(newConfig.ip, ip.c_str(), sizeof(newConfig.ip) - 1);
                newConfig.ip[sizeof(newConfig.ip) - 1] = '\0';
                newConfig.ipType = Core::Api::Utils::detectIpType(ip);

                if (!ServerConfig::isValid(newConfig))
                    return std::unexpected{"invalid config!"};

                it->second->stop();
                it->second->join();
                servers_.erase(it);
            }
        }

        if (nameOnly) {
            Context::get().logs().info(
                Db::LogCategory::Server,
                "SERVER_RENAMED",
                std::format("[{}] renamed to '{}' (no restart, sessions preserved)", oldName, newName)
            );
            return {};
        }

        std::expected<void,std::string> ok = createServer(type, newConfig);
        if (!ok) {
            Context::get().logs().error(
                Db::LogCategory::Server,
                "SERVER_UPDATE_RECREATE_FAIL",
                std::format("[{}] update failed: could not recreate server as '{}' (ip='{}', port={})",
                    oldName, newName, ip, port)
            );
            return ok;
        }

        Context::get().logs().info(
            Db::LogCategory::Server,
            "SERVER_UPDATED",
            std::format("[{}] updated to name='{}', ip='{}', port={} (restarted, sessions dropped)",
                oldName, newName, ip, port),
            newConfig.toJsonStr()
        );
        return {};
    }
} // namespace Raptor::Core::Servers
