#pragma once
#include <cstdint>
#include <drogon/WebSocketController.h>
#include <json/value.h>
#include <mutex>
#include <optional>
namespace Raptor::Core::Api {
// Commands the client can send over the WebSocket.
// Parsed from the first byte of every incoming binary frame.
enum class WsCmd : uint8_t {
    Error = 0xFE, // server → client only; never sent by client
    Unknown ,
};
// Incoming message from the client.
// T is the command-specific payload type.
template <typename T>
struct WsRequest {
    WsCmd command;
    T payload;
};
// Whether the server completed the command successfully.
enum class WsStatus : uint8_t { Ok, Error };
// Outgoing message from the server.
// T is the command-specific data type returned on success.
// Two distinct error levels:
//   - error field set → infrastructure / parse failure (bad command, internal fault)
//   - data field set  → command ran but reported a domain-level problem (e.g. agent unreachable)
template <typename T>
struct WsResponse {
    WsCmd command;
    WsStatus status;
    uint32_t payloadLen;
    T data;  // populated on Ok
    std::optional<std::string> error; // populated on Error; absent on Ok
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
    // Validates the token cookie; rejects and closes if absent.
    // Stores the connection pointer under mutex_.
    void handleNewConnection(const drogon::HttpRequestPtr&,
                             const drogon::WebSocketConnectionPtr&) override;
    // Called for every complete frame received from the client.
    // Reads cmd byte [0], validates against WsCmd, then dispatches.
    void handleNewMessage(const drogon::WebSocketConnectionPtr&,
                          std::string&&,
                          const drogon::WebSocketMessageType&) override;
    // Called when the client disconnects or the connection is dropped.
    // Clears connection_ under mutex_ so push methods are a no-op.
    void handleConnectionClosed(const drogon::WebSocketConnectionPtr&) override;
private:

    void sendJsonOk(const drogon::WebSocketConnectionPtr& conn, WsCmd cmd, const Json::Value& data) noexcept;
    void sendErrorJson(const drogon::WebSocketConnectionPtr& conn, WsCmd cmd, const std::string& reason) noexcept;

    // Builds a binary frame from raw parts.
    // Layout: [cmd(1)][status(1)][payloadLen big-endian(4)][payload]
    static std::string buildFrame(WsCmd cmd,WsStatus status,const void* payload,uint64_t payloadLen);
    // Overload for WsResponse — delegates to the raw version.
    template <typename T>
    static std::string buildFrame(const WsResponse<T>& response, const void* payload)
    {
        return buildFrame(response.command, response.status, payload, response.payloadLen);
    }
    // Sends a WsStatus::Ok frame with an optional payload.
    static void sendOk(const drogon::WebSocketConnectionPtr& conn,WsCmd cmd,const void* payload= nullptr,uint64_t payloadLen = 0);
    // Sends a WsStatus::Error frame; reason is UTF-8 encoded in the payload.
    // cmd identifies which command triggered the error.
    static void sendError(const drogon::WebSocketConnectionPtr& conn,WsCmd cmd,const std::string& reason);
    // At most one client is connected at a time.
    // All access must be guarded by mutex_.
    static inline drogon::WebSocketConnectionPtr connection_;
    static inline std::mutex  mutex_;
};
} // namespace Raptor::Core::Api
