#pragma once
#include "base.hpp"
#include "core/session/manager.hpp"
#include "type.hpp"
#include <cstdint>
#include <vector>

namespace Raptor::Core::Servers {

    /**
     * @brief Snapshot of a server's current state for reporting to the UI.
     *
     * Populated from a running server instance.
     */
    struct ServerInfo {
        const ServerConfig        config;          // Server configuration (ip, port, name, etc.).
        const ServerStatus        status;          // Current lifecycle status.
        const std::string         error;           // Last error message, empty if none.
        Common::Types::ServerType type;            // Protocol type (TCP, UDP, etc.).
        const size_t              sessionCounter;  // Number of active sessions.
        uint64_t                  uptimeSeconds{}; // Seconds since server started.
        const uint64_t            rxBytes{};       // Total bytes received.
        const uint64_t            txBytes{};       // Total bytes sent.
    };

    using ServersInfoList = std::vector<ServerInfo>;

    struct ServerEntry {
        std::string                    name;             // Human-readable server identifier
        std::string                    ipAddress;        // Bound IP address
        uint16_t                       port;             // Listening port
        Common::Types::ServerType      type;             // Protocol / role (e.g. TCP, UDP, proxy)
        ServerStatus                status;           // Current state of the server
        size_t                         sessionCount;     // Number of sessions currently hosted
        uint64_t                       bytesSent;        // Total outbound bytes since server start
        uint64_t                       bytesReceived;    // Total inbound bytes since server start
        Common::Types::Clock::duration       startTime;        // Timestamp when the server was started

    };

    struct ServerPoolStatus {
        size_t                   runningServerCount;  // Number of servers currently active and running
        size_t                   totalServerCount;    // Total number of servers (running + stopped + idle)
        size_t                   totalSessionCount;   // Total sessions across all servers, regardless of type or state
        size_t                   activeSessionCount;  // Sessions currently in use / actively handling traffic
        size_t                   totalBytesReceived;  // Cumulative inbound traffic across all servers, in bytes
        size_t                   totalBytesSent;      // Cumulative outbound traffic across all servers, in bytes
        std::vector<ServerEntry> servers;             // Per-server breakdown for all entries in the pool
    };



    /**
     * @brief Owns and manages all running server instances.
     *
     * Each server is identified by its instance name from ServerConfig.
     * Servers are created, started, paused, resumed, and stopped through this manager.
     */
    class Manager {
    public:
        Manager()  = default;
        ~Manager();

        /**
         * @brief Creates and starts a server of the given type.
         *
         * @param type    Protocol type (TCP, UDP, etc.).
         * @param config  Server configuration.
         * @return        true if the server was created and started successfully.
         */
        bool createServer(Common::Types::ServerType type, ServerConfig config);

        /**
         * @brief Stops and removes a server by instance name.
         *
         * @param name  Instance name from ServerConfig.
         * @return      true if found and stopped.
         */
        bool stopServer(const std::string& name);

        /** @brief Pauses a running server by instance name. */
        bool pauseServer(const std::string& name);

        /** @brief Resumes a paused server by instance name. */
        bool resumeServer(const std::string& name);

        /** @brief Returns the config of a server by instance name. */
        const ServerConfig getServerConfig(const std::string& name) const noexcept;

        /** @brief Returns true if the server is currently running. */
        bool isServerRunning(const std::string& name) const noexcept;

        /** @brief Returns true if the server has a recorded error. */
        bool hasError(const std::string& name) const noexcept;

        /** @brief Returns the current status of a server by instance name. */
        ServerStatus getServerStatus(const std::string& name) const noexcept;

        /**
         * @brief Returns rx/tx byte counters for a server.
         *
         * @param name     Instance name.
         * @param rxBytes  Out: total bytes received.
         * @param txBytes  Out: total bytes sent.
         */
        void getServerMetrics(const std::string& name, uint64_t& rxBytes, uint64_t& txBytes) const noexcept;

        /** @brief Stops all managed servers. */
        void stopAll() noexcept;

        /** @brief Blocks until all managed servers have finished. */
        void joinAll() noexcept;

        /**
         * @brief Returns a snapshot list of all servers' current state.
         *
         * Safe to read from any thread — no live references held.
         */
        ServersInfoList getServersInfo() const noexcept;

        /** @brief Returns the number of managed servers. */
        [[nodiscard]] std::size_t count() const noexcept;

        bool hasServer(const std::string& name) const noexcept;

        Server::SessionManager* getSessionManager(const std::string& name) const  noexcept;
        /**
         * @brief Returns a full snapshot of the pool: aggregate counters
         *        and a per-server breakdown. Safe to call from any thread.
         */
        ServerPoolStatus getServerPoolStatus() const noexcept;
    private:
        /// Owns all server instances — key is instance name.
        std::unordered_map<std::string, std::unique_ptr<Base>> servers_;

        mutable std::shared_mutex mutex_;
    };

} // namespace Raptor::Core::Servers
