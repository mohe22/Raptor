#pragma once
#include "base.hpp"
#include "core/session/manager.hpp"
#include "type.hpp"
#include <cstdint>
#include <expected>
#include <vector>

namespace Raptor::Core::Servers {

    /**
     * @brief Snapshot of a single server's current state for UI reporting.
     *
     * Populated from a live server instance on demand — no live references held.
     */
    struct ServerInfo {
        const ServerConfig config;// Server configuration (ip, port, name, etc.).
        const ServerStatus status;// Current lifecycle status.
        const std::string  error; // Last error message, empty if none.
        Common::Types::ServerType type;  // Protocol type (TCP, UDP, etc.).
        const size_t  sessionCounter;  // Number of active sessions.
        uint64_t uptimeSeconds{}; // Seconds since server started.
        const uint64_t rxBytes{}; // Total bytes received.
        const uint64_t txBytes{}; // Total bytes sent.
    };

    using ServersInfoList = std::vector<ServerInfo>;

    /**
     * @brief Flat server entry used in pool status and dashboard views.
     *
     * Cheaper than ServerInfo — no config copy, just the fields the UI needs.
     */
    struct ServerEntry {
        std::string name;          // Human-readable server identifier.
        std::string ipAddress;     // Bound IP address.
        uint16_t port;          // Listening port.
        Common::Types::ServerType type;          // Protocol / role (TCP, UDP, etc.).
        ServerStatus status;        // Current lifecycle state.
        size_t sessionCount;  // Sessions currently hosted.
        uint64_t bytesSent;     // Total outbound bytes since start.
        uint64_t bytesReceived; // Total inbound bytes since start.
        Common::Types::Clock::duration startTime;     // Duration since server started.
    };

    /**
     * @brief Aggregate snapshot of the entire server pool.
     *
     * Contains pool-wide counters and a per-server breakdown.
     * Returned by getServerPoolStatus() — safe to read from any thread.
     */
    struct ServerPoolStatus {
        size_t runningServerCount; // Servers currently active and running.
        size_t totalServerCount;   // All servers regardless of state.
        size_t totalSessionCount;  // Sessions across all servers.
        size_t activeSessionCount; // Sessions on running servers only.
        size_t totalBytesReceived; // Cumulative inbound traffic, in bytes.
        size_t totalBytesSent;     // Cumulative outbound traffic, in bytes.
        std::vector<ServerEntry> servers; // Per-server breakdown.
    };

     /**
     * @brief Owns and manages all running server instances.
     *
     * Each server is identified by its instance name from ServerConfig.
     * Thread-safe via shared_mutex — reads are concurrent, writes are exclusive.
     *
     * Lifetime:
     *   Servers are created via createServer() and destroyed on stopServer()
     *   or stopAll(). The destructor calls stopAll() automatically.
     */
    class Manager {
    public:
        Manager()  = default;
        ~Manager();

        /**
         * @brief Creates and starts a server of the given type.
         *
         * Validates the config, constructs the appropriate server type,
         * starts it, then registers it under its instance name.
         *
         * @param type    Protocol type (TCP, UDP, etc.).
         * @param config  Server configuration including name, ip, and port.
         * @return        expected if success return nothing if error return the reason.
         */
        std::expected<void,std::string> createServer(Common::Types::ServerType type, ServerConfig config) ;

        /**
         * @brief Stops and removes a server by instance name.
         *
         * Ownership is transferred out of the map before stop() is called,
         * so the server is destroyed after the lock is released.
         *
         * @param name  Instance name from ServerConfig.
         * @return      true if found and stopped, false if not found.
         */
        bool stopServer(const std::string& name);

        /**
         * @brief Pauses a running server by instance name.
         *
         * @param name  Instance name.
         * @return      true if found and paused.
         */
        bool pauseServer(const std::string& name);

        /**
         * @brief Resumes a paused server by instance name.
         *
         * @param name  Instance name.
         * @return      true if found and resumed.
         */
        bool resumeServer(const std::string& name);

        /**
         * @brief Returns the config of a server by instance name.
         *
         * @param name  Instance name.
         * @return      Copy of ServerConfig, or default-constructed if not found.
         */
        const ServerConfig getServerConfig(const std::string& name) const noexcept;

        /**
         * @brief Returns true if the server is currently in the Running state.
         *
         * @param name  Instance name.
         */
        bool isServerRunning(const std::string& name) const noexcept;

        /**
         * @brief Returns true if the server has a recorded error.
         *
         * @param name  Instance name.
         */
        bool hasError(const std::string& name) const noexcept;

        /**
         * @brief Returns the current lifecycle status of a server.
         *
         * @param name  Instance name.
         * @return      ServerStatus, or Stopped if not found.
         */
        ServerStatus getServerStatus(const std::string& name) const noexcept;

        /**
         * @brief Returns rx/tx byte counters for a server.
         *
         * @param name     Instance name.
         * @param rxBytes total bytes received.
         * @param txBytes total bytes sent.
         */
        void getServerMetrics(const std::string& name, uint64_t& rxBytes, uint64_t& txBytes) const noexcept;

        /**
         * @brief Stops all managed servers and clears the registry.
         *
         * Called automatically by the destructor.
         */
        void stopAll() noexcept;

        /**
         * @brief Blocks until all managed servers have finished their threads.
         */
        void joinAll() noexcept;

        /**
         * @brief Returns a snapshot list of all servers' current state.
         *
         * Safe to read from any thread — returns copies, no live references.
         */
        ServersInfoList getServersInfo() const noexcept;

        /**
         * @brief Returns the number of managed servers.
         */
        [[nodiscard]] std::size_t count() const noexcept;

        /**
         * @brief Returns true if a server with the given name is registered.
         *
         * @param name  Instance name.
         */
        bool hasServer(const std::string& name) const noexcept;

        /**
         * @brief Returns a raw pointer to a server's SessionManager.
         *
         * Used by API controllers to query sessions for a specific server.
         * Returns nullptr if the server is not found.
         *
         * @param name  Instance name.
         * @return      Non-owning pointer, or nullptr if not found.
         */
        Server::SessionManager* getSessionManager(const std::string& name) const noexcept;

        /**
         * @brief Returns a full snapshot of the pool.
         *
         * Aggregates counters across all servers and includes a per-server
         * breakdown in ServerPoolStatus::servers. Safe to call from any thread.
         *
         * @return  ServerPoolStatus with aggregate and per-server data.
         */
        ServerPoolStatus getServerPoolStatus() const noexcept;

        /**
         * @brief Updates the IP or port of an existing server.
         *
         * Builds and starts a new server with the updated config before touching the
         * existing one. Only if the new server starts successfully, the old server is
         * swapped out of the map and shut down cleanly. If the new server fails to
         * start, the existing server remains running and untouched.
         *
         * All active sessions on the old server are dropped and the old port is
         * released after the swap.
         *
         * @param name  Instance name of the server to update.
         * @param port  New listening port (1–65535).
         * @param ip    New binding IP address (IPv4 or IPv6).
         * @return      void on success, error string on failure.
         */
        std::expected<void, std::string> updateServer(const std::string& name,
                                                      const uint16_t     port,
                                                      const std::string& ip);

        std::optional<ServerInfo> getServerInfo(const std::string& name) const noexcept;
        private:
        /// Owns all server instances  key is instance name.
        std::unordered_map<std::string, std::unique_ptr<Base>> servers_;

        mutable std::shared_mutex mutex_; /// Protects servers_  shared reads, exclusive writes.
    };

} // namespace Raptor::Core::Servers
