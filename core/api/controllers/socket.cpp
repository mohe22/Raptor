#include "core/api/controllers/socket.hpp"
#include "core/api/utils.hpp"
#include <format>

namespace Raptor::Core::Api {


void WebSocket::sendJsonOk(
    const drogon::WebSocketConnectionPtr& conn,
    WsCmd cmd,
    const Json::Value& data) noexcept
{
    if (!conn || !conn->connected()) return;

    Json::Value response;
    response["command"] = static_cast<int>(cmd);
    response["status"]  = static_cast<int>(WsStatus::Ok);
    response["data"]    = data;

    conn->send(response.toStyledString(), drogon::WebSocketMessageType::Text);
}

void WebSocket::sendErrorJson(
    const drogon::WebSocketConnectionPtr& conn,
    WsCmd cmd,
    const std::string& reason) noexcept
{
    if (!conn || !conn->connected()) return;

    Json::Value response;
    response["command"] = static_cast<int>(cmd);
    response["status"]  = static_cast<int>(WsStatus::Error);
    response["error"]   = reason;

    conn->send(response.toStyledString(), drogon::WebSocketMessageType::Text);
}


void WebSocket::handleNewConnection(
    const drogon::HttpRequestPtr& req,
    const drogon::WebSocketConnectionPtr& conn)
{
    // Reject the upgrade immediately if no auth token is present.
    if (req->getCookie("token").empty()) {
        conn->forceClose();
        return;
    }

    std::lock_guard lock(mutex_);
    connection_ = conn;
}

void WebSocket::handleConnectionClosed(const drogon::WebSocketConnectionPtr&) {
    std::lock_guard lock(mutex_);
    connection_ = nullptr;
    std::println("[ws] client disconnected");
}


void WebSocket::handleNewMessage(
    const drogon::WebSocketConnectionPtr& conn,
    std::string&& msg,
    const drogon::WebSocketMessageType& type)
{
    if (type == drogon::WebSocketMessageType::Text) {
        handleJsonMessage(conn, msg);
        return;
    }


    sendErrorJson(conn, WsCmd::Error, "binary frames are not supported");
}

void WebSocket::handleJsonMessage(
    const drogon::WebSocketConnectionPtr& conn,
    const std::string& msg)
{
    Json::Value root;
    Json::CharReaderBuilder builder;
    std::string errs;
    const std::unique_ptr<Json::CharReader> reader(builder.newCharReader());

    if (!reader->parse(msg.data(), msg.data() + msg.size(), &root, &errs)) {
        sendErrorJson(conn, WsCmd::Error, std::format("invalid json: {}", errs));
        return;
    }

    // Every message must carry a numeric "command" field.
    if (!root.isMember("command") || !root["command"].isUInt()) {
        sendErrorJson(conn, WsCmd::Error, "missing or invalid 'command' field");
        return;
    }

    const auto cmdByte = static_cast<uint8_t>(root["command"].asUInt());

    if (!Utils::isValidWsCmd(cmdByte)) {
        sendErrorJson(conn, WsCmd::Error, std::format("unknown command: 0x{:02x}", cmdByte));
        return;
    }

    const WsCmd     cmd     = static_cast<WsCmd>(cmdByte);
    const Json::Value payload = root.isMember("payload")
        ? root["payload"]
        : Json::Value(Json::objectValue);

    dispatchCommand(conn, cmd, payload);
}
void WebSocket::dispatchNewSession(
    const std::string& username,
    const std::string& hostname,
    const std::string& timezone,
    const std::string& id,
    const std::string& connectedAt,
    const std::string& os,
    const std::string& serverId
) noexcept
{
    Json::Value data;
    data["id"]  = id;
    data["username"]= username;
    data["hostname"] = hostname;
    data["timezone"]   = timezone;
    data["connectedAt"] = connectedAt;
    data["os"]  = os;
    data["serverId"] = serverId;

   sendJsonOk(connection_, WsCmd::NewSession, data);
}
void WebSocket::dispatchCommand(
    const drogon::WebSocketConnectionPtr& conn,
    WsCmd cmd,
    const Json::Value&)
{
    switch (cmd) {
        default:
            sendErrorJson(conn, cmd,
                std::format("unhandled command: 0x{:02x}", static_cast<uint8_t>(cmd)));
            break;
    }
}


} // namespace Raptor::Core::Api
