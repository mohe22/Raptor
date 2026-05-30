#pragma once

#include "base.hpp"

#include "core/session/base.hpp"

#include <string>
#include <memory>
#include <unordered_map>

namespace Raptor::Core::Servers {

    struct ServerInfo {
        const Raptor::Core::Servers::ServerConfig config;
        const Raptor::Core::Servers::ServerStatus status;
        const std::string error;
        const size_t sessionCounter;
        uint64_t uptimeSeconds{};
        const uint64_t rxBytes{};
        const uint64_t txBytes{};
    };
    using ServersInfoList = std::vector<ServerInfo>;

    class Manager {
        public:
            Manager() = default;
            ~Manager();

            /**
             * @brief Factory: creates and starts a server by type.
             * @return true if the server was created and started successfully.
             */
            bool createServer(Session::Type type, ServerConfig config);

            /**
             * @brief Stops and removes a server by name.
             */
            bool stopServer(const std::string& name);

            bool pauseServer(const std::string& name);
            bool resumeServer(const std::string& name);

            const ServerConfig getServerConfig(const std::string& name) const noexcept;
            bool isServerRunning(const std::string& name) const noexcept;
            bool hasError(const std::string& name) const noexcept;
            ServerStatus getServerStatus(const std::string& name) const noexcept;
            void getServerMetrics(const std::string& name, uint64_t& rxBytes, uint64_t& txBytes) const noexcept;
            void stopAll() noexcept;
            void joinAll() noexcept;

            ServersInfoList getServersInfo() const noexcept;

            [[nodiscard]] std::size_t count() const noexcept;

        private:
            std::unordered_map<std::string, std::unique_ptr<Base>> servers_;
};

} // namespace Raptor::Core::Servers
