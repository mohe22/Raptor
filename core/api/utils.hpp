#pragma once
#include <drogon/utils/Utilities.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <json/json.h>
#include <chrono>
#include <string>

namespace Raptor::Core::Api::Utils {

inline std::string base64UrlEncode(const unsigned char* data, size_t len) {
    std::string b64 = drogon::utils::base64Encode(data, len);
    std::replace(b64.begin(), b64.end(), '+', '-');
    std::replace(b64.begin(), b64.end(), '/', '_');
    b64.erase(std::remove(b64.begin(), b64.end(), '='), b64.end());
    return b64;
}

inline std::string base64UrlDecode(const std::string& in) {
    std::string out = in;
    std::replace(out.begin(), out.end(), '-', '+');
    std::replace(out.begin(), out.end(), '_', '/');
    while (out.size() % 4) out += '=';
    return drogon::utils::base64Decode(out);
}

inline std::string hmacSha256Base64Url(const std::string& data, const std::string& secret) {
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digestLen = 0;
    HMAC(EVP_sha256(),
         secret.data(), static_cast<int>(secret.size()),
         reinterpret_cast<const unsigned char*>(data.data()), data.size(),
         digest, &digestLen);
    return base64UrlEncode(digest, digestLen);
}

inline std::string buildJwt(const std::string& subject,
                             const std::string& issuer,
                             const std::string& secret) {
    // Header
    std::string headerJson = R"({"alg":"HS256","typ":"JWT"})";
    std::string header = base64UrlEncode(
        reinterpret_cast<const unsigned char*>(headerJson.data()), headerJson.size());

    // Payload
    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    Json::Value payload;
    payload["sub"] = subject;
    payload["iss"] = issuer;
    payload["iat"] = static_cast<Json::Int64>(now);
    payload["exp"] = static_cast<Json::Int64>(now + (60 * 60 * 24 * 7)); // 7 days

    Json::StreamWriterBuilder writer;
    writer["indentation"] = "";
    std::string payloadJson = Json::writeString(writer, payload);
    std::string payloadB64 = base64UrlEncode(
        reinterpret_cast<const unsigned char*>(payloadJson.data()), payloadJson.size());

    // Signature
    std::string signingInput = header + "." + payloadB64;
    return signingInput + "." + hmacSha256Base64Url(signingInput, secret);
}

inline bool verifyJwt(const std::string& token,
                      const std::string& secret,
                      const std::string& issuer,
                      std::string& outUserId) {
    auto parts = drogon::utils::splitString(token, ".");
    if (parts.size() != 3) return false;

    // Verify algorithm
    std::string headerJson = base64UrlDecode(parts[0]);
    Json::Value header;
    std::string errs;
    Json::CharReaderBuilder builder;
    std::istringstream hs(headerJson);
    if (!Json::parseFromStream(builder, hs, &header, &errs)) return false;
    if (header["alg"].asString() != "HS256") return false;

    // Verify signature
    std::string expected = hmacSha256Base64Url(parts[0] + "." + parts[1], secret);
    if (expected != parts[2]) return false;

    // Decode payload
    std::string payloadJson = base64UrlDecode(parts[1]);
    Json::Value payload;
    std::istringstream ps(payloadJson);
    if (!Json::parseFromStream(builder, ps, &payload, &errs)) return false;

    // Check expiry
    if (payload.isMember("exp")) {
        auto exp = payload["exp"].asInt64();
        auto now = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        if (now > exp) return false;
    }

    // Check issuer
    if (payload.isMember("iss") && payload["iss"].asString() != issuer) return false;

    outUserId = payload["sub"].asString();
    return true;
}

} // namespace Raptor::Utils
