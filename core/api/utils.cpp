

#include "core/api/utils.hpp"
#include <print>

namespace Raptor::Core::Api::Utils {
     std::string base64UrlEncode(const unsigned char* data, size_t len) noexcept {
        std::string b64 = drogon::utils::base64Encode(data, len);
        std::replace(b64.begin(), b64.end(), '+', '-');
        std::replace(b64.begin(), b64.end(), '/', '_');
        b64.erase(std::remove(b64.begin(), b64.end(), '='), b64.end());
        return b64;
    }

    std::string base64UrlDecode(const std::string& in)  noexcept{
        std::string out = in;
        std::replace(out.begin(), out.end(), '-', '+');
        std::replace(out.begin(), out.end(), '_', '/');
        while (out.size() % 4) out += '=';
        return drogon::utils::base64Decode(out);
    }
    Json::Value sessionToJson(const Raptor::Core::Session::Base& s, const std::string& serverName) noexcept {
        Json::Value item;
        item["id"] =  std::to_string(s.id());
        item["protocol"] = Raptor::Common::Types::ToString(s.type());
        item["status"]  = Raptor::Core::Session::ToString(s.status());
        item["idleSeconds"] = static_cast<Json::UInt64>(s.idleSeconds());
        item["uptimeSeconds"]= static_cast<Json::UInt64>(s.uptimeSeconds());
        item["remoteAddress"] = s.getAddressStr();
        item["connectedTo"]= serverName;

        const auto& r = s.getRegistrationInfo();

        item["hostname"]= r.hostname;
        item["username"]= r.username;
        item["homeDir"]= r.homeDir;
        item["shell"] = r.shell;
        item["isAdmin"] = r.isAdmin;
        item["isSudoer"] = r.isSudoer;
        item["domain"] = r.domain;

        item["os"] = r.os;
        item["osVersion"] = r.osVersion;
        item["kernelVersion"] = r.kernelVersion;
        item["arch"] = r.arch;
        item["cpu"]= r.cpu;
        item["cpuCores"] = static_cast<Json::UInt>(r.cpuCores);
        item["ramBytes"]  = static_cast<Json::UInt64>(r.ramBytes);
        item["diskTotalBytes"]= static_cast<Json::UInt64>(r.diskTotalBytes);
        item["diskFreeBytes"] = static_cast<Json::UInt64>(r.diskFreeBytes);
        item["pid"] = static_cast<Json::UInt>(r.pid);
        item["processPath"]   = r.processPath;
        item["processName"]   = r.processName;
        item["uptimeSystem"]  = static_cast<Json::UInt64>(r.uptimeSeconds);

        item["isDocker"] = r.isDocker;
        item["isVM"] = r.isVM;
        item["vmType"] = r.vmType;

        item["internalIp"] = r.internalIp;
        item["internalIp2"] = r.internalIp2;
        item["macAddress"] = r.macAddress;
        item["defaultGateway"]  = r.defaultGateway;
        item["dnsServer"]  = r.dnsServer;
        item["isProxy"]  = r.isProxy;
        item["proxyUrl"] = r.proxyUrl;
        item["isDomainJoined"]  = r.isDomainJoined;

        item["selinuxEnabled"]  = r.selinuxEnabled;
        item["apparmorEnabled"] = r.apparmorEnabled;

        item["timezone"]  = r.timezone;
        item["locale"]  = r.locale;

        return item;
    }

    std::string hmacSha256Base64Url(const std::string& data, const std::string& secret) noexcept{
        unsigned char digest[EVP_MAX_MD_SIZE];
        unsigned int digestLen = 0;
        HMAC(EVP_sha256(),
             secret.data(), static_cast<int>(secret.size()),
             reinterpret_cast<const unsigned char*>(data.data()), data.size(),
             digest, &digestLen);
        return base64UrlEncode(digest, digestLen);
    }

    std::string buildJwt(const std::string& subject,
                                 const std::string& issuer,
                                 const std::string& secret) noexcept{
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

     bool verifyJwt(const std::string& token,const std::string& secret, const std::string& issuer,std::string& outUserId) noexcept {
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
     Net::IPType detectIpType(const std::string& ip) noexcept {
        struct sockaddr_in6 sa6{};
        if (inet_pton(AF_INET6, ip.c_str(), &sa6.sin6_addr) == 1)
            return Net::IPType::IPv6;
        return Net::IPType::IPv4;
    }

    Json::Value ToIdentityJson(const Common::Register& reg) noexcept
     {
         Json::Value identity;
         identity["hostname"]   = reg.hostname;
         identity["username"]   = reg.username;
         identity["homeDir"]    = reg.homeDir;
         identity["shell"]      = reg.shell;
         identity["isAdmin"]    = reg.isAdmin;
         identity["isSudoer"]   = reg.isSudoer;
         identity["domain"]     = reg.domain;
         return identity;
     }
     Json::Value ToSystemJson(const Common::Register& reg) noexcept
     {
         Json::Value system;
         system["os"]             = reg.os;
         system["osVersion"]      = reg.osVersion;
         system["kernelVersion"]  = reg.kernelVersion;
         system["arch"]           = reg.arch;
         system["cpu"]            = reg.cpu;
         system["cpuCores"]       = reg.cpuCores;
         system["ramBytes"]       = static_cast<Json::UInt64>(reg.ramBytes);
         system["diskTotalBytes"] = static_cast<Json::UInt64>(reg.diskTotalBytes);
         system["diskFreeBytes"]  = static_cast<Json::UInt64>(reg.diskFreeBytes);
         system["pid"]            = reg.pid;
         system["processPath"]    = reg.processPath;
         system["processName"]    = reg.processName;
         system["uptimeSeconds"]  = static_cast<Json::UInt64>(reg.uptimeSeconds);
         return system;
     }

     Json::Value ToVirtualizationJson(const Common::Register& reg) noexcept
     {
         Json::Value virt;
         virt["isDocker"] = reg.isDocker;
         virt["isVM"]     = reg.isVM;
         virt["vmType"]   = reg.vmType;
         return virt;
     }

     Json::Value ToNetworkJson(const Common::Register& reg) noexcept
     {
         Json::Value network;
         network["internalIp"]     = reg.internalIp;
         network["internalIp2"]    = reg.internalIp2;
         network["macAddress"]     = reg.macAddress;
         network["defaultGateway"] = reg.defaultGateway;
         network["dnsServer"]      = reg.dnsServer;
         network["isProxy"]        = reg.isProxy;
         network["proxyUrl"]       = reg.proxyUrl;
         network["isDomainJoined"] = reg.isDomainJoined;
         return network;
     }

     Json::Value ToSecurityJson(const Common::Register& reg) noexcept
     {
         Json::Value security;
         security["selinuxEnabled"]  = reg.selinuxEnabled;
         security["apparmorEnabled"] = reg.apparmorEnabled;
         return security;
     }

     Json::Value ToMiscJson(const Common::Register& reg) noexcept
     {
         Json::Value misc;
         misc["timezone"]    = reg.timezone;
         misc["locale"]      = reg.locale;
         return misc;
     }
     Json::Value ConvertToJson(const Common::Register& reg) noexcept
     {
         Json::Value root;

         root["identity"]       = ToIdentityJson(reg);
         root["system"]         = ToSystemJson(reg);
         root["virtualization"] = ToVirtualizationJson(reg);
         root["network"]        = ToNetworkJson(reg);
         root["security"]       = ToSecurityJson(reg);
         root["misc"]           = ToMiscJson(reg);

         return root;
     }

     Json::Value ToLogEntryJson(const std::vector<Db::LogEntry>& entries) noexcept {
         Json::Value root(Json::arrayValue);

         for (const auto& ent : entries) {
             Json::Value item;

             item["id"]      = ent.id;
             item["ts"]      = static_cast<Json::UInt64>(ent.ts);
             item["level"]   = Db::toString(ent.level);
             item["category"]     = Db::toString(ent.category);
             item["event"]   = ent.event;
             item["message"] = ent.message;
             item["meta"]    = ent.meta;

             root.append(item);
         }

         return root;
     }


     Json::Value ToPoolStatusJson(const Raptor::Core::Servers::ServerPoolStatus& pool,const std::vector<Raptor::Core::Db::LogEntry>& serverLogs,const std::vector<Raptor::Core::Db::LogEntry>& sessionLogs) noexcept{
         Json::Value json;

          // Pool statistics
          json["runningServerCount"] = static_cast<Json::UInt64>(pool.runningServerCount);
          json["totalServerCount"] = static_cast<Json::UInt64>(pool.totalServerCount);
          json["totalSessionCount"] = static_cast<Json::UInt64>(pool.totalSessionCount);
          json["activeSessionCount"] = static_cast<Json::UInt64>(pool.activeSessionCount);
          json["totalBytesReceived"] = static_cast<Json::UInt64>(pool.totalBytesReceived);
          json["totalBytesSent"] = static_cast<Json::UInt64>(pool.totalBytesSent);

          // Logs
          json["serversLogs"] = ToLogEntryJson(serverLogs);
          json["sessionLogs"] = ToLogEntryJson(sessionLogs);

          // Servers array
          Json::Value serversArray(Json::arrayValue);
          for (const auto& s : pool.servers) {
              Json::Value entry;
              entry["name"] = s.name;
              entry["ipAddress"] = s.ipAddress;
              entry["port"] = s.port;
              entry["type"] = Raptor::Common::Types::ToString(s.type);
              entry["status"] = Raptor::Core::Servers::ToString(s.status);
              entry["sessionCount"] = static_cast<Json::UInt64>(s.sessionCount);
              entry["bytesSent"] = static_cast<Json::UInt64>(s.bytesSent);
              entry["bytesReceived"] = static_cast<Json::UInt64>(s.bytesReceived);
              entry["startTime"] = static_cast<Json::Int64>(std::chrono::duration_cast<std::chrono::seconds>(s.startTime).count());
              serversArray.append(std::move(entry));
          }
          json["servers"] = std::move(serversArray);

          return json;
     }

}
