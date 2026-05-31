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
        if (!servers_.contains(name))
            return {};
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

        if (servers_.contains(config.instanceName))
            return false; // already exists

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

        Context::get().logs().info(
            Db::LogCategory::Server,
            "SERVER_CREATED",
            std::format("[{}] {} server created", config.instanceName, Common::Types::ToString(type)),
            config.toJsonStr()
        );

        if (!server->start()) {
            Context::get().logs().error(
                Db::LogCategory::Server,
                "SERVER_START_FAIL",
                std::format("[{}] Failed to start: {}", config.instanceName, server->getErrorMsg())
            );
            return false;
        }

        servers_.emplace(config.instanceName, std::move(server));
        return true;
    }

    bool Manager::stopServer(const std::string& name) {
        auto it = servers_.find(name);
        if (it == servers_.end()) return false;

        it->second->stop();

        Context::get().logs().info(
            Db::LogCategory::Server,
            "SERVER_STOP",
            std::format("[{}] server stopped", name)
        );

        servers_.erase(it);
        return true;
    }

    bool Manager::pauseServer(const std::string& name) {
        auto it = servers_.find(name);
        if (it == servers_.end()) return false;

        it->second->pause();

        Context::get().logs().info(
            Db::LogCategory::Server,
            "SERVER_PAUSE",
            std::format("[{}] server paused", name)
        );

        return true;
    }

    bool Manager::resumeServer(const std::string& name) {
        auto it = servers_.find(name);
        if (it == servers_.end()) return false;

        it->second->resume();

        Context::get().logs().info(
            Db::LogCategory::Server,
            "SERVER_RESUME",
            std::format("[{}] server resumed", name)
        );

        return true;
    }

    bool Manager::isServerRunning(const std::string& name) const noexcept {
        auto it = servers_.find(name);
        return it != servers_.end() && it->second->isRunning();
    }

    bool Manager::hasError(const std::string& name) const noexcept {
        auto it = servers_.find(name);
        return it != servers_.end() && it->second->hasError();
    }

    ServerStatus Manager::getServerStatus(const std::string& name) const noexcept {
        auto it = servers_.find(name);
        return it != servers_.end() ? it->second->status() : ServerStatus::Stopped;
    }

    void Manager::stopAll() noexcept {
        for (auto& [name, server] : servers_) {
            server->stop();
        }
        servers_.clear();
    }

    void Manager::joinAll() noexcept {
        for (auto& [name, server] : servers_) {
            server->join();
        }
    }

    std::size_t Manager::count() const noexcept {
        return servers_.size();
    }

    ServersInfoList Manager::getServersInfo() const noexcept {
        ServersInfoList info;
        for (const auto& [name, server] : servers_) {
            uint64_t rx = 0, tx = 0;
            server->getTxBytes(tx);
            server->getRxBytes(rx);
            info.emplace_back(ServerInfo{
                server->config(),
                server->status(),
                server->getErrorMsgStr(),
                server->type(),
                server->sessionManager.count(),
                server->uptimeSeconds(),
                rx,
                tx
            });
        }
        return info;
    }

} // namespace Raptor::Core::Servers
