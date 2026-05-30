#include "auth.hpp"
#include "core/api/utils.hpp"
#include "core/config.hpp"
#include <json/json.h>

void Auth::login(const HttpRequestPtr &req,
                 std::function<void(const HttpResponsePtr &)> &&callback) {
    auto body = req->getJsonObject();
    if (!body) {
        Json::Value err;
        err["message"] = "Invalid JSON body";
        auto resp = HttpResponse::newHttpJsonResponse(err);
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }

    std::string username = (*body).get("username", "").asString();
    std::string password = (*body).get("password", "").asString();

    if (username.empty() || password.empty()) {
        Json::Value err;
        err["message"] = "Username and password are required";
        auto resp = HttpResponse::newHttpJsonResponse(err);
        resp->setStatusCode(k400BadRequest);
        callback(resp);
        return;
    }
    // TODO: finde another way to gener
    if (username != Raptor::Core::Config::USERNAME || password != Raptor::Core::Config::PASSWORD) {
        Json::Value err;
        err["message"] = "Invalid credentials";
        auto resp = HttpResponse::newHttpJsonResponse(err);
        resp->setStatusCode(k401Unauthorized);
        callback(resp);
        return;
    }

    std::string token = Raptor::Core::Api::Utils::buildJwt(username,
                                                 Raptor::Core::Config::JWT_ISSUER,
                                                 Raptor::Core::Config::JWT_SECRET);
    Json::Value result;
    result["userId"] = username;

    auto resp = HttpResponse::newHttpJsonResponse(result);
    Cookie cookie("token", token);
    cookie.setHttpOnly(true);
    cookie.setSecure(false); // true in production
    cookie.setMaxAge(60 * 60 * 24 * 7);
    cookie.setPath("/");
    cookie.setSameSite(Cookie::SameSite::kLax);
    resp->addCookie(cookie);
    callback(resp);
}

void Auth::me(const HttpRequestPtr &req,
              std::function<void(const HttpResponsePtr &)> &&callback) {
    std::string userId = req->getAttributes()->get<std::string>("userId");
    Json::Value result;
    result["userId"] = userId;
    callback(HttpResponse::newHttpJsonResponse(result));
}
