#include "session.hpp"
#include "core/api/utils.hpp"
#include "core/context.hpp"
#include "core/session/base.hpp"
#include <json/value.h>
#include <print>
#include <string>



void Sessions::getSessionForServer(const HttpRequestPtr& , std::function<void(const HttpResponsePtr&)>&& callback, const std::string& serverName) {
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

        Json::Value json(Json::arrayValue);
        for (const auto& s : sessionsInfo) {
            Json::Value item;
            item["id"] = std::to_string(s.id);
            item["protocol"]
                = Raptor::Common::Types::ToString(s.protocol);
            item["status"]  = Raptor::Core::Session::ToString(s.status);
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

void Sessions::getSessionById(const HttpRequestPtr&, std::function<void(const HttpResponsePtr&)>&& callback, const std::string& serverName, const std::string& sessionId) {
    try {
        uint64_t id;
        try {
            id = std::stoull(sessionId);
        } catch (const std::invalid_argument&) {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k400BadRequest);
            resp->setBody("Invalid session id");
            callback(resp);
            return;
        } catch (const std::out_of_range&) {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k400BadRequest);
            resp->setBody("Session id out of range");
            callback(resp);
            return;
        }

        const auto& servers = Raptor::Core::Context::get().servers();
        const Raptor::Core::Server::SessionManager* sessionManager = servers.getSessionManager(serverName);
        if (!sessionManager) {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k404NotFound);
            resp->setBody("Server not found");
            callback(resp);
            return;
        }

        const auto* session = sessionManager->find(id);
        if (!session) {
            auto resp = drogon::HttpResponse::newHttpResponse();
            resp->setStatusCode(drogon::k404NotFound);
            resp->setBody("Session Id was not found!");
            callback(resp);
            return;
        }

        callback(drogon::HttpResponse::newHttpJsonResponse(Raptor::Core::Api::Utils::sessionToJson(*session, serverName)));
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
