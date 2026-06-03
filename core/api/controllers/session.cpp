#include "session.hpp"
#include "core/context.hpp"
#include <json/value.h>



void Sessions::getSessionForServer(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback, const std::string& serverName) {
    try {
        const auto& servers = Raptor::Core::Context::get().servers();
        const Raptor::Core::Server::SessionManager* sessionManager = servers.getSessionManager(serverName);

        if (!sessionManager) {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k404NotFound);
            resp->setBody("Server not found");
            callback(resp);
            return;
        }

        const auto& sessionsInfo = sessionManager->getSessionsInfo();
        std::println("size: {}",sessionsInfo.size());

        Json::Value json(Json::arrayValue);
        for (const auto& s : sessionsInfo) {
            Json::Value item;
            item["id"]            = static_cast<Json::UInt64>(s.id);
            item["protocol"]      = Raptor::Common::Types::ToString(s.protocol);
            item["status"]        = Raptor::Core::Session::ToString(s.status);
            item["idleSeconds"]   = static_cast<Json::UInt64>(s.idleSeconds);
            item["remoteAddress"] = s.remoteAddress;
            item["connectedTo"] = serverName;
            item["os"] = s.os;
            item["hostname"] = s.hostname;
            item["username"] = s.username;
            item["timezone"] = s.timezone;
            json.append(std::move(item));
        }

        callback(drogon::HttpResponse::newHttpJsonResponse(json));
    }
    catch (const std::exception& e) {
        Json::Value errorJson;
        errorJson["error"] = e.what();
        auto resp = drogon::HttpResponse::newHttpJsonResponse(errorJson);
        resp->setStatusCode(drogon::k500InternalServerError);
        callback(resp);
    }
    catch (...) {
        Json::Value errorJson;
        errorJson["error"] = "Unknown server error";
        auto resp = drogon::HttpResponse::newHttpJsonResponse(errorJson);
        resp->setStatusCode(drogon::k500InternalServerError);
        callback(resp);
    }
}
