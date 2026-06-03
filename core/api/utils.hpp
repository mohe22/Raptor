#pragma once
#include "core/db/logRepository.hpp"
#include "register.hpp"
#include <drogon/utils/Utilities.h>
#include <openssl/hmac.h>
#include <openssl/evp.h>
#include <json/json.h>

namespace Raptor::Core::Api::Utils {
    std::string base64UrlEncode(const unsigned char* data, size_t len) noexcept;
    std::string base64UrlDecode(const std::string& in) noexcept;
    std::string hmacSha256Base64Url(const std::string& data, const std::string& secret) noexcept;
    std::string buildJwt(const std::string& subject, const std::string& issuer,const std::string& secret)  noexcept;
    bool verifyJwt(const std::string& token,const std::string& secret, const std::string& issuer,std::string& outUserId) noexcept;

    Json::Value ToIdentityJson(const Common::Register& reg) noexcept;
    Json::Value ToSystemJson(const Common::Register& reg) noexcept;
    Json::Value ToVirtualizationJson(const Common::Register& reg) noexcept;
    Json::Value ToNetworkJson(const Common::Register& reg)noexcept;
    Json::Value ToSecurityJson(const Common::Register& reg) noexcept;
    Json::Value ToMiscJson(const Common::Register& reg) noexcept;
    Json::Value ConvertToJson(const Common::Register& reg) noexcept;
    Json::Value ToLogEntryJson(const std::vector<Db::LogEntry>&) noexcept;
} // namespace Raptor::Utils
