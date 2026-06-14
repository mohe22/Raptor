#include "core/api/controllers/socket.hpp"
#include "core/api/utils.hpp"

namespace Raptor::Core::Api {
    std::string WebSocket::buildFrame(WsCmd cmd,WsStatus status,const void* payload,uint64_t payloadLen){
        constexpr std::size_t HEADER = 6;
        std::string frame(HEADER + payloadLen, '\0');
        frame[0] = static_cast<uint8_t>(cmd);
        frame[1] = static_cast<uint8_t>(status);
        // Encode payloadLen as 4 bytes big-endian (network byte order).
        for (int i = 0; i < 4; ++i)
            frame[2 + i] = static_cast<char>((payloadLen >> (24 - 8 * i)) & 0xFF);

        if (payload && payloadLen > 0)
            std::memcpy(frame.data() + HEADER, payload, payloadLen);
        return frame;
    }
    void WebSocket::sendOk(const drogon::WebSocketConnectionPtr& conn,WsCmd cmd,const void* payload,uint64_t  payloadLen){
        conn->send(buildFrame(cmd, WsStatus::Ok, payload, payloadLen), drogon::WebSocketMessageType::Binary);
    }
    void WebSocket::sendError(const drogon::WebSocketConnectionPtr& conn,WsCmd cmd,const std::string&reason){
        conn->send(buildFrame(cmd, WsStatus::Error, reason.data(), reason.size()),
            drogon::WebSocketMessageType::Binary);
    }
    void WebSocket::sendJsonOk(const drogon::WebSocketConnectionPtr& conn,WsCmd cmd,const Json::Value& data) noexcept
        {
            if (!conn || !conn->connected()) {
                return;
            }

            Json::Value response;
            response["command"] = static_cast<int>(cmd);
            response["status"]  = static_cast<int>(WsStatus::Ok);
            response["data"]    = data;

            std::string payload = response.toStyledString();
            conn->send(payload, drogon::WebSocketMessageType::Text);
        }
    void WebSocket::handleNewConnection(const drogon::HttpRequestPtr& req,const drogon::WebSocketConnectionPtr& conn){
        const std::string token = req->getCookie("token");
        if (token.empty()) { conn->forceClose(); return; }
        {
            std::lock_guard lock(mutex_);
            connection_ = conn;
        }
    }
    void WebSocket::sendErrorJson(const drogon::WebSocketConnectionPtr& conn, WsCmd cmd, const std::string& reason) noexcept
        {
            if (!conn || !conn->connected()) return;

            Json::Value response;
            response["command"] = static_cast<uint8_t>(cmd);
            response["status"]  = static_cast<uint8_t>(WsStatus::Error);
            response["error"]   = reason;

            std::string payload = response.toStyledString();
            conn->send(payload, drogon::WebSocketMessageType::Text);
        }
    void WebSocket::handleNewMessage(const drogon::WebSocketConnectionPtr& conn,std::string&& msg,const drogon::WebSocketMessageType& type){

        if(type == drogon::WebSocketMessageType::Text){
            Json::Value root;
            Json::CharReaderBuilder readerBuilder;
            std::string errs;

            const std::unique_ptr<Json::CharReader> reader(readerBuilder.newCharReader());

            if (!reader->parse(msg.data(), msg.data() + msg.size(), &root, &errs)) {
                sendError(conn, WsCmd::Error, "invalid json payload: " + errs);
                return;
            }

            if (!root.isMember("command") || !root["command"].isUInt()) {
                sendError(conn, WsCmd::Error, "missing or invalid 'cmd' field");
                return;
            }

            const uint8_t cmdByte = static_cast<uint8_t>(root["command"].asUInt());

            if (!Utils::isValidWsCmd(cmdByte)) {
                sendError(conn, WsCmd::Error, std::format("unknown command: 0x{:02x}", cmdByte));
                return;
            }

            const WsCmd cmd = static_cast<WsCmd>(cmdByte);

            Json::Value data = root["data"].isObject() ? root["data"] : Json::Value(Json::objectValue);

            switch (cmd) {
                      default:
                sendError(conn, cmd, std::format("unhandled command: 0x{:02x}", cmdByte));
                break;
            }
        }


    }
    void WebSocket::handleConnectionClosed(const drogon::WebSocketConnectionPtr&)
    {
        std::lock_guard lock(mutex_);
        connection_ = nullptr;
        std::println("[ws] client disconnected");
    }

} // namespace Raptor::Core::Api
