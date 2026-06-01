#pragma once
#include <cstdint>
#include <cstring>
#include <expected>
#include <linux/limits.h>
#include <netinet/in.h>
#include <span>
#include <vector>

namespace Raptor::Common {

    struct Register {
        // Identity
        char     hostname[256];
        char     username[128];
        char     homeDir[256];               // e.g. /home/mohe
        char     shell[64];                  // e.g. /bin/bash
        bool     isAdmin;
        bool     isSudoer;                   // member of sudo/wheel group
        char     domain[128];

        // System
        char     os[64];
        char     osVersion[64];
        char     kernelVersion[128];         // full kernel version string
        char     arch[16];
        char     cpu[128];
        uint32_t cpuCores;
        uint64_t ramBytes;
        uint64_t diskTotalBytes;             // total disk space
        uint64_t diskFreeBytes;              // free disk space
        uint32_t pid;
        char     processPath[PATH_MAX];
        char     processName[256];
        uint64_t uptimeSeconds;              // system uptime

        // Virtualization
        bool     isDocker;
        bool     isVM;                       // hypervisor flag detected
        char     vmType[64];                 // e.g. "VMware", "KVM", "VirtualBox"

        // Network
        char     internalIp[INET6_ADDRSTRLEN];
        char     internalIp2[INET6_ADDRSTRLEN];
        char     macAddress[18];
        char     defaultGateway[INET6_ADDRSTRLEN];
        char     dnsServer[INET6_ADDRSTRLEN];  // primary DNS from resolv.conf
        bool     isProxy;
        char     proxyUrl[512];
        bool     isDomainJoined;

        // Security
        bool     selinuxEnabled;
        bool     apparmorEnabled;

        // Misc
        char     timezone[64];
        char     locale[32];
        int64_t  firstSeenAt;

        [[nodiscard]] std::vector<uint8_t> serialize() const noexcept {
            std::vector<uint8_t> buf(sizeof(Register));
            std::memcpy(buf.data(), this, sizeof(Register));
            return buf;
        }

        [[nodiscard]] static std::expected<Register, const char*>
        deserialize(std::span<const uint8_t> buf) noexcept {
            if (buf.size() < sizeof(Register))
                return std::unexpected{"buffer too small"};
            Register reg{};
            std::memcpy(&reg, buf.data(), sizeof(Register));
            return reg;
        }
    };

} // namespace Raptor::Common
