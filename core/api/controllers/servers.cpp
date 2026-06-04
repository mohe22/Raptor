#include "servers.hpp"
#include "core/api/utils.hpp"
#include "core/context.hpp"
#include "type.hpp"

void Server::getPoolStatus(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
    try {
        auto& servers = Raptor::Core::Context::get().servers();
        const auto& logs = Raptor::Core::Context::get().logs();
        const Raptor::Core::Servers::ServerPoolStatus pool = servers.getServerPoolStatus();


        Raptor::Core::Db::Result<std::vector<Raptor::Core::Db::LogEntry>> entries = logs.get({
            .category = Raptor::Core::Db::LogCategory::Server,
            .limit = 10
        });

        if (!entries.has_value())
            throw std::runtime_error(std::format("message: {},code:{}",entries.error().message,entries.error().code));
        Raptor::Core::Db::Result<std::vector<Raptor::Core::Db::LogEntry>> sessionEntries = logs.get({
               .category = Raptor::Core::Db::LogCategory::Session,
               .limit = 10
        });
        if (!sessionEntries.has_value())
            throw std::runtime_error(std::format("message: {},code:{}",entries.error().message,entries.error().code));

        Json::Value json;
        json["runningServerCount"]  = static_cast<Json::UInt64>(pool.runningServerCount);
        json["totalServerCount"]    = static_cast<Json::UInt64>(pool.totalServerCount);
        json["totalSessionCount"]   = static_cast<Json::UInt64>(pool.totalSessionCount);
        json["activeSessionCount"]  = static_cast<Json::UInt64>(pool.activeSessionCount);
        json["totalBytesReceived"]  = static_cast<Json::UInt64>(pool.totalBytesReceived);
        json["totalBytesSent"]      = static_cast<Json::UInt64>(pool.totalBytesSent);
        json["serversLogs"] = Raptor::Core::Api::Utils::ToLogEntryJson(entries.value());
        json["sessionLogs"] = Raptor::Core::Api::Utils::ToLogEntryJson(sessionEntries.value());

        Json::Value serversArray(Json::arrayValue);
        for (const auto& s : pool.servers) {
            Json::Value entry;
            entry["name"]          = s.name;
            entry["ipAddress"]     = s.ipAddress;
            entry["port"]          = s.port;
            entry["type"]          = Raptor::Common::Types::ToString(s.type);
            entry["status"]        = Raptor::Core::Servers::ToString(s.status);
            entry["sessionCount"]  = static_cast<Json::UInt64>(s.sessionCount);
            entry["bytesSent"]     = static_cast<Json::UInt64>(s.bytesSent);
            entry["bytesReceived"] = static_cast<Json::UInt64>(s.bytesReceived);
            entry["startTime"]     = static_cast<Json::Int64>(
                std::chrono::duration_cast<std::chrono::seconds>(s.startTime).count()
            );
            serversArray.append(std::move(entry));
        }
        json["servers"] = std::move(serversArray);

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

void Server::getServers( const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback){
    try {
        auto& servers = Raptor::Core::Context::get().servers();
        const Raptor::Core::Servers::ServersInfoList& serversInfo = servers.getServersInfo();

        Json::Value json(Json::arrayValue);



        for (const auto& s : serversInfo) {
            Json::Value item;

            Json::Value config;
            config["ip"] = s.config.ip;
            config["port"] = s.config.port;
            config["ipType"] = (s.config.ipType == Net::IPType::IPv4) ? "IPv4" : "IPv6";
            config["timeout"] = s.config.epollTimeout;
            config["instanceName"] = s.config.instanceName;

            item["config"] = std::move(config);
            item["type"] = Raptor::Common::Types::ToString(s.type);
            item["status"] = Raptor::Core::Servers::ToString(s.status);
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

void Server::createServer(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
    try {
        auto body = req->getJsonObject();
        if (!body) {
            Json::Value err;
            err["error"] = "invalid or missing JSON body";
            auto resp = drogon::HttpResponse::newHttpJsonResponse(err);
            resp->setStatusCode(drogon::k400BadRequest);
            return callback(resp);
        }

        if (!body->isMember("name") || !body->isMember("port") ||
            !body->isMember("ip")   || !body->isMember("type")) {
            Json::Value err;
            err["error"] = "missing required fields: name, port, ip, type";
            auto resp = drogon::HttpResponse::newHttpJsonResponse(err);
            resp->setStatusCode(drogon::k400BadRequest);
            return callback(resp);
        }

        const std::string name    = (*body)["name"].asString();
        const std::string ip      = (*body)["ip"].asString();
        const std::string typeStr = (*body)["type"].asString();
        const int portNum = (*body)["port"].asInt();

        if (name.empty() || name.size() > 50) {
            Json::Value err;
            err["error"] = "name: required, max 50 chars";
            auto resp = drogon::HttpResponse::newHttpJsonResponse(err);
            resp->setStatusCode(drogon::k400BadRequest);
            return callback(resp);
        }

        if (portNum < 1 || portNum > 65535) {
            Json::Value err;
            err["error"] = "port: must be between 1 and 65535";
            auto resp = drogon::HttpResponse::newHttpJsonResponse(err);
            resp->setStatusCode(drogon::k400BadRequest);
            return callback(resp);
        }

        const auto serverType = Raptor::Common::Types::FromString(typeStr);
        if (!serverType) {
            Json::Value err;
            err["error"] = "type: must be TCP, UDP, or HTTP";
            auto resp = drogon::HttpResponse::newHttpJsonResponse(err);
            resp->setStatusCode(drogon::k400BadRequest);
            return callback(resp);
        }

        auto& servers = Raptor::Core::Context::get().servers();

        if (servers.hasServer(name)) {
            Json::Value err;
            err["error"] = std::format("server '{}' already exists", name);
            auto resp = drogon::HttpResponse::newHttpJsonResponse(err);
            resp->setStatusCode(drogon::k409Conflict);
            return callback(resp);
        }

        Raptor::Core::Servers::ServerConfig config;
        config.instanceName = name;
        config.port         = static_cast<uint16_t>(portNum);
        config.ipType       = Raptor::Core::Api::Utils::detectIpType(ip);
        config.epollTimeout = 2000;
        strncpy(config.ip, ip.c_str(), sizeof(config.ip) - 1);
        config.ip[sizeof(config.ip) - 1] = '\0';

        const bool ok = servers.createServer(serverType.value(), config);

        Json::Value json;
        json["success"] = ok;
        if (!ok) {
            json["error"] = "failed to create server, name may already exist or config is invalid";
            auto resp = drogon::HttpResponse::newHttpJsonResponse(json);
            resp->setStatusCode(drogon::k409Conflict);
            return callback(resp);
        }

        json["message"] = std::format("server '{}' created", config.instanceName);
        callback(drogon::HttpResponse::newHttpJsonResponse(json));
    }
    catch (const std::exception& e) {
        Json::Value err;
        err["error"] = e.what();
        auto resp = drogon::HttpResponse::newHttpJsonResponse(err);
        resp->setStatusCode(drogon::k500InternalServerError);
        callback(resp);
    }
    catch (...) {
        Json::Value err;
        err["error"] = "unknown server error";
        auto resp = drogon::HttpResponse::newHttpJsonResponse(err);
        resp->setStatusCode(drogon::k500InternalServerError);
        callback(resp);
    }
}
