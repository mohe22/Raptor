#include "AuthFilter.hpp"
#include <drogon/utils/Utilities.h>
#include <json/json.h>

#include "core/config.hpp"

#include <openssl/hmac.h>
#include <openssl/evp.h>

static std::string base64UrlDecode(const std::string& in) {
    std::string out = in;
    std::replace(out.begin(), out.end(), '-', '+');
    std::replace(out.begin(), out.end(), '_', '/');
    while (out.size() % 4) out += '=';
    return drogon::utils::base64Decode(out);
}

static std::string hmacSha256Base64Url(const std::string& data, const std::string& secret) {
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digestLen = 0;
    HMAC(EVP_sha256(),
         secret.data(), static_cast<int>(secret.size()),
         reinterpret_cast<const unsigned char*>(data.data()), data.size(),
         digest, &digestLen);

    // base64 encode
    std::string raw(reinterpret_cast<char*>(digest), digestLen);
    std::string b64 = drogon::utils::base64Encode(
        reinterpret_cast<const unsigned char*>(raw.data()), raw.size());

    // make url-safe
    std::replace(b64.begin(), b64.end(), '+', '-');
    std::replace(b64.begin(), b64.end(), '/', '_');
    b64.erase(std::remove(b64.begin(), b64.end(), '='), b64.end());
    return b64;
}

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

    try {
        // Split header.payload.signature
        auto parts = drogon::utils::splitString(token, ".");
        if (parts.size() != 3) throw std::runtime_error("Invalid token format");

        // Verify signature
        std::string expected = hmacSha256Base64Url(parts[0] + "." + parts[1],
                                                    Raptor::Core::Config::JWT_SECRET);
        if (expected != parts[2]) throw std::runtime_error("Invalid signature");

        // Decode payload
        std::string payloadJson = base64UrlDecode(parts[1]);

        Json::Value payload;
        std::string errs;
        Json::CharReaderBuilder builder;
        std::istringstream ss(payloadJson);
        if (!Json::parseFromStream(builder, ss, &payload, &errs))
            throw std::runtime_error("Invalid payload");

        // Check expiry
        if (payload.isMember("exp")) {
            auto exp = payload["exp"].asInt64();
            auto now = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();
            if (now > exp) throw std::runtime_error("Token expired");
        }

        // Check issuer
        if (payload.isMember("iss") &&
            payload["iss"].asString() != Raptor::Core::Config::JWT_ISSUER)
            throw std::runtime_error("Invalid issuer");

        req->getAttributes()->insert("userId", payload["sub"].asString());
        fccb();

    } catch (const std::exception &e) {
        Json::Value err;
        err["message"] = "Invalid or expired token";
        auto resp = HttpResponse::newHttpJsonResponse(err);
        resp->setStatusCode(k401Unauthorized);
        fcb(resp);
    }
}
