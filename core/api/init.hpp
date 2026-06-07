#pragma once
#include <drogon/HttpAppFramework.h>
#include <algorithm>
#include <string>
#include <vector>
#include "core/config.hpp"

namespace Raptor::Core::Api {
    inline void init() {

        static const std::vector<std::string> allowedOrigins(
            Config::ALLOWED_ORIGINS,
            Config::ALLOWED_ORIGINS + Config::ALLOWED_ORIGINS_COUNT
        );

        drogon::app().registerSyncAdvice(
            [](const drogon::HttpRequestPtr& req) -> drogon::HttpResponsePtr {
                if (req->method() != drogon::Options)
                    return nullptr;
                std::string origin = req->getHeader("Origin");
                auto it = std::find(
                    Config::ALLOWED_ORIGINS,
                    Config::ALLOWED_ORIGINS + Config::ALLOWED_ORIGINS_COUNT,
                    origin
                );
                auto resp = drogon::HttpResponse::newHttpResponse();
                if (it != Config::ALLOWED_ORIGINS + Config::ALLOWED_ORIGINS_COUNT) {
                    resp->addHeader("Access-Control-Allow-Origin", origin);
                    resp->addHeader("Access-Control-Allow-Credentials", "true");
                }
                resp->addHeader("Access-Control-Allow-Methods",
                                "GET, POST, PUT, DELETE, OPTIONS");
                resp->addHeader("Access-Control-Allow-Headers",
                                "Content-Type, Authorization, X-Requested-With");
                resp->addHeader("Access-Control-Max-Age", "86400");
                resp->setStatusCode(drogon::k204NoContent);
                return resp;
            });

        drogon::app().registerPostHandlingAdvice(
            [](const drogon::HttpRequestPtr& req,
               const drogon::HttpResponsePtr& resp) {
                std::string origin = req->getHeader("Origin");
                auto it = std::find(
                    Config::ALLOWED_ORIGINS,
                    Config::ALLOWED_ORIGINS + Config::ALLOWED_ORIGINS_COUNT,
                    origin
                );
                if (it != Config::ALLOWED_ORIGINS + Config::ALLOWED_ORIGINS_COUNT) {
                    resp->addHeader("Access-Control-Allow-Origin", origin);
                    resp->addHeader("Access-Control-Allow-Credentials", "true");
                }
                resp->addHeader("Access-Control-Allow-Methods",
                                "GET, POST, PUT, DELETE, OPTIONS");
                resp->addHeader("Access-Control-Allow-Headers",
                                "Content-Type, Authorization, X-Requested-With");
                resp->addHeader("Vary", "Origin");
            });

        auto notFound = drogon::HttpResponse::newHttpResponse();
        notFound->setStatusCode(drogon::k404NotFound);
        notFound->setBody("Not Found");
        drogon::app().setCustom404Page(notFound);
        drogon::app().addListener(Config::BACKEND_IP, Config::BACKEND_PORT);
    }

    inline void run() {
        drogon::app()
            .setThreadNum(Config::BACKEND_THREADS)
            .setLogLevel(Config::BACKEND_LOG
                  ? trantor::Logger::kDebug
                  : trantor::Logger::kFatal)
            .run();
    }
}
