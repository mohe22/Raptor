#pragma once
#include <cstdint>
#include <drogon/WebSocketController.h>
#include <json/value.h>
#include <mutex>

namespace Raptor::Core::Api {

// Commands the client can send over the WebSocket.
// The "command" field in the JSON frame identifies the type.
enum class WsCmd : uint8_t {
    SessionConnected = 0x1,
    SessionDisconnected = 0x2,
    Unknown    = 0xFF,
};

// Included in every response so the client can distinguish success from failure.
// it tell us if the server faild or not
enum class WsStatus : uint8_t { Ok = 0x00, Error = 0x01 };

class WebSocket : public drogon::WebSocketController<WebSocket> {
public:
    WS_PATH_LIST_BEGIN
    WS_PATH_ADD("/ws", drogon::Get);
    WS_PATH_LIST_END

    // Validates the token cookie on upgrade and stores the connection pointer.
    // Closes immediately if the cookie is absent.
    void handleNewConnection(const drogon::HttpRequestPtr&,
                             const drogon::WebSocketConnectionPtr&) override;

    // Entry point for every incoming frame.
    // Text frames are forwarded to handleJsonMessage.
    // Binary frames are rejected with WsCmd::Error.
    void handleNewMessage(const drogon::WebSocketConnectionPtr&,
                          std::string&&,
                          const drogon::WebSocketMessageType&) override;

    // Clears connection_ under mutex_ so all send helpers become no-ops.
    void handleConnectionClosed(const drogon::WebSocketConnectionPtr&) override;


    static void dispatchSessionConnected(
        const std::string& username,
        const std::string&hostname,
        const std::string&timezone,
        const std::string& id,
        const std::string& connectedAt,
        const std::string& os,
        const std::string& serverId,
        const std::string& address
    )  noexcept;
    static void dispatchSessionDisconnected(const std::string& id,const std::string& serverId) noexcept;
private:
    // Parses the raw JSON string, validates the "command" field,
    // extracts "payload", and forwards to dispatchCommand.
    void handleJsonMessage(const drogon::WebSocketConnectionPtr& conn,
                           const std::string& msg);

    // Routes a validated command to its dedicated handler.
    void dispatchCommand(const drogon::WebSocketConnectionPtr& conn,
                         WsCmd cmd,
                         const Json::Value& payload);

     // Sends { command, status: Ok, data } as a JSON text frame.
    static void sendJsonOk(const drogon::WebSocketConnectionPtr& conn,
                    WsCmd cmd,
                    const Json::Value& data) noexcept;

    // Sends { command, status: Error, error: reason } as a JSON text frame.
    static void sendErrorJson(const drogon::WebSocketConnectionPtr& conn,
                       WsCmd cmd,
                       const std::string& reason) noexcept;

    // Guarded by mutex_; nullptr when no client is connected.
    static inline drogon::WebSocketConnectionPtr connection_;
    static inline std::mutex mutex_;
};

} // namespace Raptor::Core::Api
