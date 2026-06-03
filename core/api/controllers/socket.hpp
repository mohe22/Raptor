#pragma once
#include <cstdint>
#include <drogon/WebSocketController.h>
#include <json/value.h>

namespace Raptor::Core::Api {

    // Commands the client can send over the WebSocket.
    // Parsed from the "command" field of every incoming JSON message.
    enum class WsCmd : uint8_t {
        DashboardStatus, // pull current dashboard metrics
    };

    // Incoming message from the client.
    // T is the command-specific payload type (use Json::Value for a generic handler).
    template <typename T>
    struct WsRequest {
       WsCmd command;
        T           payload; // command-specific body; may be an empty object
    };

    // Whether the server completed the command successfully.
    enum class WsStatus : uint8_t { Ok, Error };

    // Outgoing message from the server.
    // T is the command-specific data type returned on success.

    // Two distinct error levels:
    //   - error field set→ infrastructure / parse failure (bad command, internal fault)
    //   - data field set→ command ran but reported a domain-level problem (e.g. agent unreachable)
    template <typename T>
    struct WsResponse {
        WsCmd   command;
        WsStatus  status;
        uint64_t payloadLen;
        T  data;    // populated on Ok; may carry domain errors when status == Ok
        std::optional<std::string> error;   // populated on Error; absent on Ok
    };

    // Single-connection WebSocket controller mounted at /ws.
    // Only one simultaneous connection is supported (connection_ is static).
    class WebSocket : public drogon::WebSocketController<WebSocket>
    {
    public:
        WS_PATH_LIST_BEGIN
        WS_PATH_ADD("/ws", drogon::Get);
        WS_PATH_LIST_END

        // Called once when the HTTP upgrade handshake completes.
        // Stores the connection pointer and initialises per-connection state.
        void handleNewConnection(const drogon::HttpRequestPtr&,
                                 const drogon::WebSocketConnectionPtr&) override;

        // Called for every complete frame received from the client.
        // Parses the JSON body, resolves the WsCmd, and dispatches accordingly.
        void handleNewMessage(const drogon::WebSocketConnectionPtr&,
                              std::string&&,
                              const drogon::WebSocketMessageType&) override;

        // Called when the client disconnects or the connection is dropped.
        // Clears connection_ under mutex_ so sendServersStatus() is a no-op.
        void handleConnectionClosed(const drogon::WebSocketConnectionPtr&) override;

        // Pushes the current server-list snapshot to the active connection.
        // Thread-safe: acquires mutex_ before accessing connection_.
        // No-op when no client is connected.
        static void sendServersStatus() noexcept;

    private:
        // Composes and sends the dashboard metrics response for the active connection.
        void dashboardStatus();

        // At most one client is connected at a time.
        // All access must be guarded by mutex_.
        static inline drogon::WebSocketConnectionPtr connection_;
        static inline std::mutex                      mutex_;
    };

} // namespace Raptor::Core::Api
