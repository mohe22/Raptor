/*
 * TODO:
 *  Replace fixed-size char buffers with length-prefixed.
 */
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
        char hostname[256] = {};
        char username[128] = {};
        char homeDir[256] = {};               // e.g. /home/mohe
        char shell[64] = {};                  // e.g. /bin/bash
        bool isAdmin = false;
        bool isSudoer = false;                // member of sudo/wheel group
        char domain[128] = {};
        // System
        char os[64] = {};
        char osVersion[64] = {};
        char kernelVersion[128] = {};             // full kernel version string
        char arch[16] = {};
        char cpu[128] = {};
        uint32_t cpuCores = 0;
        uint64_t ramBytes = 0;
        uint64_t diskTotalBytes = 0;             // total disk space
        uint64_t diskFreeBytes = 0;              // free disk space
        uint32_t pid = 0;
        char processPath[PATH_MAX] = {};
        char processName[256] = {};
        uint64_t uptimeSeconds = 0;              // system uptime
        // Virtualization
        bool isDocker = false;
        bool isVM = false;                       // hypervisor flag detected
        char vmType[64] = {};                    // e.g. "VMware", "KVM", "VirtualBox"
        // Network
        char internalIp[INET6_ADDRSTRLEN] = {};
        char internalIp2[INET6_ADDRSTRLEN] = {};
        char macAddress[18] = {};
        char defaultGateway[INET6_ADDRSTRLEN] = {};
        char dnsServer[INET6_ADDRSTRLEN] = {};   // primary DNS from resolv.conf
        bool isProxy = false;
        char proxyUrl[512] = {};
        bool isDomainJoined = false;
        // Security
        bool selinuxEnabled = false;
        bool apparmorEnabled = false;
        // Misc
        char  timezone[64] = {};
        char locale[32] = {};

        static const size_t totalSize = 256 + 128 + 256 + 64 + 2 + 128 + 64 + 64 + 128 + 16 + 128 +
                                            4 + 8 + 8 + 8 + 4 + PATH_MAX + 256 + 8 + 2 + 64 +
                                            INET6_ADDRSTRLEN + INET6_ADDRSTRLEN + 18 + INET6_ADDRSTRLEN +
                                            INET6_ADDRSTRLEN + 1 + 512 + 3 + 64 + 32;
        [[nodiscard]] std::vector<uint8_t> serialize() const noexcept {
            std::vector<uint8_t> buf;
            buf.reserve(totalSize);

            buf.insert(buf.end(), hostname, hostname + 256);
            buf.insert(buf.end(), username, username + 128);
            buf.insert(buf.end(), homeDir, homeDir + 256);
            buf.insert(buf.end(), shell, shell + 64);

            buf.push_back(static_cast<uint8_t>(isAdmin));
            buf.push_back(static_cast<uint8_t>(isSudoer));

            buf.insert(buf.end(), domain, domain + 128);
            buf.insert(buf.end(), os, os + 64);
            buf.insert(buf.end(), osVersion, osVersion + 64);
            buf.insert(buf.end(), kernelVersion, kernelVersion + 128);
            buf.insert(buf.end(), arch, arch + 16);
            buf.insert(buf.end(), cpu, cpu + 128);

            uint32_t cpuCores_net = htonl(cpuCores);
            buf.insert(buf.end(),
                       reinterpret_cast<const uint8_t*>(&cpuCores_net),
                       reinterpret_cast<const uint8_t*>(&cpuCores_net) + 4);

            uint64_t ramBytes_net = htobe64(ramBytes);
            buf.insert(buf.end(),
                       reinterpret_cast<const uint8_t*>(&ramBytes_net),
                       reinterpret_cast<const uint8_t*>(&ramBytes_net) + 8);

            uint64_t diskTotalBytes_net = htobe64(diskTotalBytes);
            buf.insert(buf.end(),
                       reinterpret_cast<const uint8_t*>(&diskTotalBytes_net),
                       reinterpret_cast<const uint8_t*>(&diskTotalBytes_net) + 8);

            uint64_t diskFreeBytes_net = htobe64(diskFreeBytes);
            buf.insert(buf.end(),
                       reinterpret_cast<const uint8_t*>(&diskFreeBytes_net),
                       reinterpret_cast<const uint8_t*>(&diskFreeBytes_net) + 8);

            uint32_t pid_net = htonl(pid);
            buf.insert(buf.end(),
                       reinterpret_cast<const uint8_t*>(&pid_net),
                       reinterpret_cast<const uint8_t*>(&pid_net) + 4);

            buf.insert(buf.end(), processPath, processPath + PATH_MAX);
            buf.insert(buf.end(), processName, processName + 256);

            uint64_t uptimeSeconds_net = htobe64(uptimeSeconds);
            buf.insert(buf.end(),
                       reinterpret_cast<const uint8_t*>(&uptimeSeconds_net),
                       reinterpret_cast<const uint8_t*>(&uptimeSeconds_net) + 8);

            buf.push_back(static_cast<uint8_t>(isDocker));
            buf.push_back(static_cast<uint8_t>(isVM));

            buf.insert(buf.end(), vmType, vmType + 64);
            buf.insert(buf.end(), internalIp, internalIp + INET6_ADDRSTRLEN);
            buf.insert(buf.end(), internalIp2, internalIp2 + INET6_ADDRSTRLEN);
            buf.insert(buf.end(), macAddress, macAddress + 18);
            buf.insert(buf.end(), defaultGateway, defaultGateway + INET6_ADDRSTRLEN);
            buf.insert(buf.end(), dnsServer, dnsServer + INET6_ADDRSTRLEN);

            buf.push_back(static_cast<uint8_t>(isProxy));

            buf.insert(buf.end(), proxyUrl, proxyUrl + 512);

            buf.push_back(static_cast<uint8_t>(isDomainJoined));
            buf.push_back(static_cast<uint8_t>(selinuxEnabled));
            buf.push_back(static_cast<uint8_t>(apparmorEnabled));

            buf.insert(buf.end(), timezone, timezone + 64);
            buf.insert(buf.end(), locale, locale + 32);

            return buf;
        }

        [[nodiscard]] static std::expected<Register, const char*>
        deserialize(std::span<const uint8_t> buf) noexcept {
            Register reg{};
            size_t offset = 0;

            if (buf.size() < totalSize)
                return std::unexpected{"buffer too small"};

            std::memcpy(reg.hostname, buf.data() + offset, 256);
            offset += 256;
            std::memcpy(reg.username, buf.data() + offset, 128);
            offset += 128;
            std::memcpy(reg.homeDir, buf.data() + offset, 256);
            offset += 256;
            std::memcpy(reg.shell, buf.data() + offset, 64);
            offset += 64;

            reg.isAdmin = static_cast<bool>(buf[offset++]);
            reg.isSudoer = static_cast<bool>(buf[offset++]);

            std::memcpy(reg.domain, buf.data() + offset, 128);
            offset += 128;
            std::memcpy(reg.os, buf.data() + offset, 64);
            offset += 64;
            std::memcpy(reg.osVersion, buf.data() + offset, 64);
            offset += 64;
            std::memcpy(reg.kernelVersion, buf.data() + offset, 128);
            offset += 128;
            std::memcpy(reg.arch, buf.data() + offset, 16);
            offset += 16;
            std::memcpy(reg.cpu, buf.data() + offset, 128);
            offset += 128;

            uint32_t cpuCoresNet;
            std::memcpy(&cpuCoresNet, buf.data() + offset, 4);
            reg.cpuCores = ntohl(cpuCoresNet);
            offset += 4;

            uint64_t ramBytesNet;
            std::memcpy(&ramBytesNet, buf.data() + offset, 8);
            reg.ramBytes = be64toh(ramBytesNet);
            offset += 8;

            uint64_t diskTotalBytesNet;
            std::memcpy(&diskTotalBytesNet, buf.data() + offset, 8);
            reg.diskTotalBytes = be64toh(diskTotalBytesNet);
            offset += 8;

            uint64_t diskFreeBytesNet;
            std::memcpy(&diskFreeBytesNet, buf.data() + offset, 8);
            reg.diskFreeBytes = be64toh(diskFreeBytesNet);
            offset += 8;

            uint32_t pidNet;
            std::memcpy(&pidNet, buf.data() + offset, 4);
            reg.pid = ntohl(pidNet);
            offset += 4;

            // String fields
            std::memcpy(reg.processPath, buf.data() + offset, PATH_MAX);
            offset += PATH_MAX;
            std::memcpy(reg.processName, buf.data() + offset, 256);
            offset += 256;

            uint64_t uptimeSeconds_net;
            std::memcpy(&uptimeSeconds_net, buf.data() + offset, 8);
            reg.uptimeSeconds = be64toh(uptimeSeconds_net);
            offset += 8;

            reg.isDocker = static_cast<bool>(buf[offset++]);
            reg.isVM = static_cast<bool>(buf[offset++]);

            std::memcpy(reg.vmType, buf.data() + offset, 64);
            offset += 64;
            std::memcpy(reg.internalIp, buf.data() + offset, INET6_ADDRSTRLEN);
            offset += INET6_ADDRSTRLEN;
            std::memcpy(reg.internalIp2, buf.data() + offset, INET6_ADDRSTRLEN);
            offset += INET6_ADDRSTRLEN;
            std::memcpy(reg.macAddress, buf.data() + offset, 18);
            offset += 18;
            std::memcpy(reg.defaultGateway, buf.data() + offset, INET6_ADDRSTRLEN);
            offset += INET6_ADDRSTRLEN;
            std::memcpy(reg.dnsServer, buf.data() + offset, INET6_ADDRSTRLEN);
            offset += INET6_ADDRSTRLEN;

            reg.isProxy = static_cast<bool>(buf[offset++]);

            std::memcpy(reg.proxyUrl, buf.data() + offset, 512);
            offset += 512;

            reg.isDomainJoined = static_cast<bool>(buf[offset++]);
            reg.selinuxEnabled = static_cast<bool>(buf[offset++]);
            reg.apparmorEnabled = static_cast<bool>(buf[offset++]);

            std::memcpy(reg.timezone, buf.data() + offset, 64);
            offset += 64;
            std::memcpy(reg.locale, buf.data() + offset, 32);
            offset += 32;

            return reg;
        }
    };
} // namespace Raptor::Common
