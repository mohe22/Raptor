#include "servers.hpp"
#include "core/context.hpp"
#include <drogon/HttpResponse.h>
#include <string>

void Server::getServers( const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback){
    try {
        auto& servers = Raptor::Core::Context::get().servers();
        const auto& serversInfo = servers.getServersInfo();

        Json::Value json(Json::arrayValue);

        for (const auto& s : serversInfo) {
            Json::Value item;

            Json::Value config;
            config["ip"] = s.config.ip;
            config["port"] = s.config.port;
            config["ipType"] = (s.config.ipType == Net::IPType::IPv4) ? "IPv4" : "IPv6";
            config["epollTimeout"] = s.config.epollTimeout;
            config["instanceName"] = s.config.instanceName;

            item["config"] = std::move(config);

            Json::Value status;
            status["running"] = Raptor::Core::Servers::ToString(s.status);
            status["connections"] = s.sessionCounter;

            item["status"] = std::move(status);

            item["error"] = s.error;
            item["sessionCounter"] = static_cast<Json::UInt64>(s.sessionCounter);
            item["rxBytes"] = static_cast<Json::UInt64>(s.rxBytes);
            item["txBytes"] = static_cast<Json::UInt64>(s.txBytes);

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

void Server::pauseServer(const HttpRequestPtr& req,std::function<void(const HttpResponsePtr&)>&& callback,const std::string& serverName) {
    try {
        auto& servers = Raptor::Core::Context::get().servers();
        bool success = servers.pauseServer(serverName);

        Json::Value json;
        json["success"] = success;
        if(!success) {
            json["error"] = "Failed to pause server";
        }
        else {
            json["message"] = "Server paused successfully";
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

void Server::resumeServer(const HttpRequestPtr& req,std::function<void(const HttpResponsePtr&)>&& callback, const std::string& serverName){
    try {

        auto& servers = Raptor::Core::Context::get().servers();
        bool success = servers.resumeServer(serverName);

        Json::Value json;
        json["success"] = success;
        if(!success) {
            json["error"] = "Failed to resume server";
        }
        else {
            json["message"] = "Server resumed successfully";
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



void Server::stopServer(const HttpRequestPtr& req,std::function<void(const HttpResponsePtr&)>&& callback,const std::string& serverName){
    try {
        auto& servers = Raptor::Core::Context::get().servers();
        bool success = servers.stopServer(serverName);

        Json::Value json;
        json["success"] = success;
        if(!success) {
            json["error"] = "Failed to stop server";
        }
        else {
            json["message"] = "Server stopped successfully";
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
