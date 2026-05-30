#pragma once
#include <cstddef>
#include <cstdint>
#include <iterator>

namespace Raptor::Core::Config {

    /// Current server version shown in handshake and logs.
    constexpr const char* VERSION = "1.0.0";

    constexpr std::size_t RECV_BUFFER_SIZE = 4096;
    /// Size of the per-connection send buffer in bytes.
    constexpr std::size_t SEND_BUFFER_SIZE = 4096;

    /// Maximum number of simultaneous agent connections.
    constexpr std::size_t MAX_CONNECTIONS = 100;

    /// Seconds of inactivity before a connection moves Connected → Idle.
    constexpr std::size_t IDLE_SECONDS    = 60;
    /// Seconds of inactivity before an Idle connection is dropped.
    constexpr std::size_t TIMEOUT_SECONDS = 120;

    /// Port agents connect to.
    constexpr std::uint16_t BACKEND_PORT = 8000;
    /// IP the backend server binds to.
    constexpr const char*   BACKEND_IP   = "0.0.0.0";
    constexpr bool  BACKEND_LOG_LEVEL = true;
    constexpr int  BACKEND_THREADS = 0;

    inline constexpr const char* JWT_SECRET = "ubwJkgS2jsS8N96Qr868f6VCpI0F0nedWeoVp6RF82k=";

    inline constexpr const char* JWT_ISSUER  = "raptor";

    constexpr const char* ALLOWED_ORIGINS[] = {
        "http://localhost:5173",
        "http://127.0.0.1:5173"
    };
    constexpr std::size_t ALLOWED_ORIGINS_COUNT = std::size(ALLOWED_ORIGINS);

} // namespace Raptor::Core::Config
