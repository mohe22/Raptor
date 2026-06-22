#include "core/api/controllers/socket.hpp"
#include "core/api/utils.hpp"
#include "core/context.hpp"
#include "core/session/base.hpp"
#include "core/session/manager.hpp"
#include "core/session/task/task.hpp"
#include "header.hpp"
#include "utils.hpp"
#include <cstdint>
#include <format>
#include <string>

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
        Json::FastWriter writer;
        const std::string& json = writer.write(response);
        conn->send(json, drogon::WebSocketMessageType::Text);
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
        Json::FastWriter writer;
        const std::string& json = writer.write(response);
        conn->send(json, drogon::WebSocketMessageType::Text);
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

        sendErrorJson(conn, WsCmd::Unknown, "binary frames are not supported");
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
            sendErrorJson(conn, WsCmd::Unknown, std::format("invalid json: {}", errs));
            return;
        }

        // Every message must carry a numeric "command" field.
        if (!root.isMember("command") || !root["command"].isUInt()) {
            sendErrorJson(conn, WsCmd::Unknown, "missing or invalid 'command' field");
            return;
        }

        const auto cmdByte = static_cast<uint8_t>(root["command"].asUInt());

        if (!Utils::isValidWsCmd(cmdByte)) {
            sendErrorJson(conn, WsCmd::Unknown, std::format("unknown command: 0x{:02x}", cmdByte));
            return;
        }

        const WsCmd cmd  = static_cast<WsCmd>(cmdByte);
        const Json::Value& payload = root.isMember("Data")
            ? root["Data"]
            : Json::Value(Json::objectValue);

        dispatchCommand(conn, cmd, payload);
    }
    void WebSocket::dispatchSessionConnected(
        const std::string& username,
        const std::string& hostname,
        const std::string& timezone,
        const std::string& id,
        const std::string& connectedAt,
        const std::string& os,
        const std::string& serverId,
        const std::string& address
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
        data["remoteAddress"] = address;
        sendJsonOk(connection_, WsCmd::SessionConnected, data);
    }

    void WebSocket::dispatchCommandOutput(const Common::Header& header, const std::string_view output) noexcept {
        if (!connection_ || !connection_->connected()) return;

        const std::string ts = Common::getCurrentTimeISO();
        const uint16_t tsLen = static_cast<uint16_t>(ts.size());
        const uint32_t outLen = static_cast<uint32_t>(output.size());

        std::vector<uint8_t> packet;
        packet.reserve(4 + 1 + 1 + 2 + tsLen + 4 + outLen);

        // packetId (uint32_t, little-endian)
        packet.push_back((header.packetId >> 24) & 0xFF);
        packet.push_back((header.packetId >> 16) & 0xFF);
        packet.push_back((header.packetId >> 8) & 0xFF);
        packet.push_back(header.packetId & 0xFF);

        // type & flags
        packet.push_back(static_cast<uint8_t>(header.type));
        packet.push_back(static_cast<uint8_t>(header.flags));

        // timestamp length
        packet.push_back(tsLen & 0xFF);
        packet.push_back((tsLen >> 8) & 0xFF);

        // timestamp
        packet.insert(packet.end(), ts.begin(), ts.end());

        // output length
        packet.push_back(outLen & 0xFF);
        packet.push_back((outLen >> 8) & 0xFF);
        packet.push_back((outLen >> 16) & 0xFF);
        packet.push_back((outLen >> 24) & 0xFF);


        packet.insert(packet.end(), output.begin(), output.end());

        connection_->send((char*)packet.data(), packet.size(), drogon::WebSocketMessageType::Binary);
    }

    void WebSocket::dispatchSessionDisconnected(const std::string& id,const std::string& serverId) noexcept{
        Json::Value data;
        data["id"]  = id;
        data["serverId"] = serverId;
        sendJsonOk(connection_, WsCmd::SessionDisconnected, data);
    };

    void WebSocket::dispatchCommand(
        const drogon::WebSocketConnectionPtr& conn,
        WsCmd cmd,
        const Json::Value&data)
    {

        switch (cmd) {
            case WsCmd::ExecuteCommand:{
                if (!data.isMember("sessionId") || !data.isMember("serverId") || !data.isMember("command")   || !data.isMember("priority") || !data.isMember("shellType") || !data.isMember("commandId")){
                    sendErrorJson(conn, cmd, "Invalid parameters");
                    return;
                }

                const std::string& sessionIdStr = data["sessionId"].asString();

                uint64_t sessionId = 0;
                try {
                    sessionId = static_cast<uint64_t>(std::stoull(sessionIdStr));
                } catch (const std::exception&) {
                    sendErrorJson(conn, cmd, "Invalid sessionId format");
                    return;
                }

                const std::string& serverId = data["serverId"].asString();
                const Server::SessionManager* sessionManager =  Context::get().servers().getSessionManager(serverId);
                if(!sessionManager){
                    sendErrorJson(conn,cmd, "ServerId Not found!");
                    return;
                }

                Session::Base* session = sessionManager->find(sessionId);
                if(!session){
                    sendErrorJson(conn, cmd, "SessionId was not found!");
                    return;
                }
                if (!data["priority"].isInt()) {
                    sendErrorJson(conn, cmd, "Invalid priority type");
                    return;
                }

                int priorityRaw = data["priority"].asInt();
                if (priorityRaw < 0 || priorityRaw > 3) {
                    sendErrorJson(conn, cmd, "Priority out of range");
                    return;
                }

                const Tasks::TaskPriority priority =
                    static_cast<Tasks::TaskPriority>(priorityRaw);

                if (!data["shellType"].isString()) {
                    sendErrorJson(conn, cmd, "Invalid shellType type");
                    return;
                }

                const std::string& shellTypeStr = data["shellType"].asString();
                if (shellTypeStr != "pip" && shellTypeStr != "pty") {
                    sendErrorJson(conn, cmd, "Invalid shellType (expected: pip | pty)");
                    return;
                }

                int commandId = data["commandId"].asInt();

                const std::string& cmd = data["command"].asString();

                Common::PacketType type =
                    shellTypeStr == "pty"
                    ? Common::PacketType::PtyCommand
                    : Common::PacketType::Command;

                session->sendCommand(
                    cmd,
                    commandId,
                    type,
                    priority
                );

                break;
            }
            default:
            sendErrorJson(conn, cmd,
                std::format("unhandled command: 0x{:02x}", static_cast<uint8_t>(cmd)));
            break;
        }
    }


} // namespace Raptor::Core::Api
