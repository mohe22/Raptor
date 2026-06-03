#include "core/api/controllers/socket.hpp"
#include <cstring>
#include <print>
#include <string>
#include <variant>

namespace Raptor::Core::Api {

template <typename T>
static std::string buildFrame(const WsResponse<T>& response,
                               const void*          payload)
{
    constexpr std::size_t HEADER = 10;
    std::string frame(HEADER + response.payloadLen, '\0');

    frame[0] = static_cast<uint8_t>(response.command);
    frame[1] = static_cast<uint8_t>(response.status);

    for (int i = 0; i < 8; ++i)
        frame[2 + i] = static_cast<char>((response.payloadLen >> (56 - 8 * i)) & 0xFF);

    if (payload && response.payloadLen > 0)
        std::memcpy(frame.data() + HEADER, payload, response.payloadLen);

    return frame;
}

void WebSocket::handleNewConnection(const drogon::HttpRequestPtr&         req,
                                    const drogon::WebSocketConnectionPtr& conn)
{
    const std::string token = req->getCookie("token");
    if (token.empty()) { conn->forceClose(); return; }
    std::println("socket token: {}", token);

    {
        std::lock_guard lock(mutex_);
        connection_ = conn;
    }

    WsResponse<std::monostate> response{
        .command    = WsCmd::DashboardStatus,
        .status     = WsStatus::Ok,
        .payloadLen = token.size(),
        .data       = {},
        .error      = std::nullopt,
    };

    conn->send(buildFrame(response, token.data()), drogon::WebSocketMessageType::Binary);
}

void WebSocket::handleNewMessage(const drogon::WebSocketConnectionPtr& conn,
                                 std::string&&                          msg,
                                 const drogon::WebSocketMessageType&    type)
{
    if (type != drogon::WebSocketMessageType::Text) return;

    WsResponse<std::monostate> response{
        .command    = WsCmd::DashboardStatus,
        .status     = WsStatus::Ok,
        .payloadLen = msg.size(),
        .data       = {},
        .error      = std::nullopt,
    };

    conn->send(buildFrame(response, msg.data()), drogon::WebSocketMessageType::Binary);
}

void WebSocket::handleConnectionClosed(const drogon::WebSocketConnectionPtr&)
{
    std::lock_guard lock(mutex_);
    connection_ = nullptr;
}

void WebSocket::sendServersStatus() noexcept
{
    std::lock_guard lock(mutex_);
    if (!connection_) return;

    const uint8_t placeholder[] = { 0x00, 0x00, 0x00, 0x00 };

    WsResponse<std::monostate> response{
        .command    = WsCmd::DashboardStatus,
        .status     = WsStatus::Ok,
        .payloadLen = sizeof(placeholder),
        .data       = {},
        .error      = std::nullopt,
    };

    try {
        connection_->send(buildFrame(response, placeholder), drogon::WebSocketMessageType::Binary);
    } catch (const std::exception& e) {
        LOG_ERROR << e.what();
    }
}

void WebSocket::dashboardStatus()
{
    std::lock_guard lock(mutex_);
    if (!connection_) return;

    const uint8_t payload[] = { 0xCA, 0xFE };

    WsResponse<std::monostate> response{
        .command    = WsCmd::DashboardStatus,
        .status     = WsStatus::Ok,
        .payloadLen = sizeof(payload),
        .data       = {},
        .error      = std::nullopt,
    };

    connection_->send(buildFrame(response, payload), drogon::WebSocketMessageType::Binary);
}

} // namespace Raptor::Core::Api
