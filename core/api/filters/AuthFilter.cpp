#include "AuthFilter.hpp"
#include "core/api/utils.hpp"
#include "core/config.hpp"

void AuthFilter::doFilter(const HttpRequestPtr &req, FilterCallback &&fcb, FilterChainCallback &&fccb) {
    std::string token = req->getCookie("token");
    if (token.empty()) {
        Json::Value err;
        err["message"] = "Not authenticated";
        auto resp = HttpResponse::newHttpJsonResponse(err);
        resp->setStatusCode(k401Unauthorized);
        fcb(resp);
        return;
    }

    std::string userId;
    if (!Raptor::Core::Api::Utils::verifyJwt(token, Raptor::Core::Config::JWT_SECRET,
                                   Raptor::Core::Config::JWT_ISSUER, userId)) {
        Json::Value err;
        err["message"] = "Invalid or expired token";
        auto resp = HttpResponse::newHttpJsonResponse(err);
        resp->setStatusCode(k401Unauthorized);
        fcb(resp);
        return;
    }

    req->getAttributes()->insert("userId", userId);
    fccb();
}
