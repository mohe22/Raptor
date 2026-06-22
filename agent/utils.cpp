#include "utils.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/statvfs.h>
#include <sys/sysinfo.h>
#include <sys/utsname.h>
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <format>
#include <fstream>
#include <string>

namespace {


/**
 * @brief Copies a C string into a fixed-size char array, always null-terminating.
 *
 * Silently truncates if @p src is longer than @p dsz - 1.
 * Does nothing if @p src is null or @p dsz is 0.
 */
 void scopy(char* dst, size_t dsz, const char* src) noexcept {
    if (!src || dsz == 0) {
            if (dst && dsz > 0) dst[0] = '\0';
            return;
        }
        // Use memcpy + manual null-termination (avoids strncpy truncation warning)
        const size_t len = std::min(strlen(src), dsz - 1);
        std::memcpy(dst, src, len);
        dst[len] = '\0';
}

/**
 * @brief Copies a string_view into a fixed-size char array, always null-terminating.
 *
 * Silently truncates if @p src is longer than @p dsz - 1.
 * Does nothing if @p dsz is 0.
 */
 void scopy(char* dst, size_t dsz, std::string_view src) noexcept {
    if (dsz == 0) return;
        if (dst) {
            const size_t len = std::min(src.size(), dsz - 1);
            std::memcpy(dst, src.data(), len);
            dst[len] = '\0';
        }
}

/**
 * @brief Returns the value of the first non-empty environment variable found.
 *
 * @param keys  Ordered list of variable names to check.
 * @return      Pointer to the value string, or nullptr if none found.
 */
 const char* env(std::initializer_list<const char*> keys) noexcept {
    for (const char* k : keys)
        if (const char* v = ::getenv(k); v && v[0]) return v;
    return nullptr;
}

} // anonymous namespace

namespace Raptor::Utils {

    void setNonBlocking(int fd) {
        int flags = ::fcntl(fd, F_GETFL, 0);
        if (flags == -1)
            throw std::runtime_error(
                std::format("fcntl(F_GETFL) fd={}: {}", fd, strerror(errno)));
        if (::fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
            throw std::runtime_error(
                std::format("fcntl(F_SETFL) fd={}: {}", fd, strerror(errno)));
    }







[[nodiscard]] Raptor::Common::Register collect() noexcept {
    Raptor::Common::Register reg{};

    ::gethostname(reg.hostname, sizeof(reg.hostname));

    scopy(reg.username, sizeof(reg.username), env({"USER", "LOGNAME"}));
    scopy(reg.homeDir, sizeof(reg.homeDir),  env({"HOME"}));
    scopy(reg.shell, sizeof(reg.shell),     env({"SHELL"}));

    reg.isAdmin = (::geteuid() == 0);

    // sudoer  check for username in sudo/wheel group lines
    {
        std::ifstream f("/etc/group");
        for (std::string line; std::getline(f, line);) {
            if ((line.starts_with("sudo:") || line.starts_with("wheel:")) &&
                line.find(reg.username) != std::string::npos) {
                reg.isSudoer = true;
                break;
            }
        }
    }

    // domain
    {
        std::ifstream f("/proc/sys/kernel/domainname");
        std::string line;
        if (f && std::getline(f, line) && line != "(none)")
            scopy(reg.domain, sizeof(reg.domain), line);
    }

    // os / kernel / arch
    {
        struct utsname u{};
        if (::uname(&u) == 0) {
            scopy(reg.os,sizeof(reg.os),u.sysname);
            scopy(reg.osVersion,sizeof(reg.osVersion),u.release);
            scopy(reg.kernelVersion, sizeof(reg.kernelVersion), u.version);
            scopy(reg.arch, sizeof(reg.arch), u.machine);
        }
    }

    // cpu model — first "model name" line from /proc/cpuinfo
    {
        std::ifstream f("/proc/cpuinfo");
        for (std::string line; std::getline(f, line);) {
            if (line.find("model name") != std::string::npos) {
                if (auto pos = line.find(':'); pos != std::string::npos)
                    scopy(reg.cpu, sizeof(reg.cpu),std::string_view(line).substr(pos + 2));
                break;
            }
        }
    }

    reg.cpuCores = static_cast<uint32_t>(::sysconf(_SC_NPROCESSORS_ONLN));

    // ram + uptime
    {
        struct sysinfo si{};
        if (::sysinfo(&si) == 0) {
            reg.ramBytes = si.totalram * si.mem_unit;
            reg.uptimeSeconds = static_cast<uint64_t>(si.uptime);
        }
    }

    // disk usage on root filesystem
    {
        struct statvfs sv{};
        if (::statvfs("/", &sv) == 0) {
            reg.diskTotalBytes = sv.f_blocks * sv.f_frsize;
            reg.diskFreeBytes= sv.f_bfree  * sv.f_frsize;
        }
    }

    reg.pid = static_cast<uint32_t>(::getpid());

    // process path + name via /proc/self/exe symlink
    {
        ssize_t len = ::readlink("/proc/self/exe",
                                 reg.processPath, sizeof(reg.processPath) - 1);
        if (len > 0) {
            reg.processPath[len] = '\0';
            const char* slash = strrchr(reg.processPath, '/');
            scopy(reg.processName, sizeof(reg.processName),
                  slash ? slash + 1 : reg.processPath);
        }
    }

    reg.isDocker = (::access("/.dockerenv", F_OK) == 0);

    // virtualization — DMI product name, then cpuinfo hypervisor flag
    {
        static constexpr struct { const char* key; const char* label; } kVendors[] = {
            { "VMware","VMware" },
            { "VirtualBox", "VirtualBox" },
            { "KVM","KVM"  },
            { "QEMU", "QEMU"},

        };

        std::ifstream f("/sys/class/dmi/id/product_name");
        std::string name;
        if (f && std::getline(f, name)) {
            for (const auto& v : kVendors) {
                if (name.find(v.key) != std::string::npos) {
                    reg.isVM = true;
                    scopy(reg.vmType, sizeof(reg.vmType), v.label);
                    break;
                }
            }
        }

        if (!reg.isVM) {
            std::ifstream cf("/proc/cpuinfo");
            for (std::string line; std::getline(cf, line);) {
                if (line.find("hypervisor") != std::string::npos) {
                    reg.isVM = true;
                    scopy(reg.vmType, sizeof(reg.vmType), "Unknown");
                    break;
                }
            }
        }
    }

    // network interfaces — up to two non-loopback IPv4 addresses
    {
        struct ifaddrs* ifaddr = nullptr;
        if (::getifaddrs(&ifaddr) == 0) {
            int count = 0;
            for (auto* ifa = ifaddr; ifa && count < 2; ifa = ifa->ifa_next) {
                if (!ifa->ifa_addr)    continue;
                if (ifa->ifa_flags & IFF_LOOPBACK) continue;
                if (ifa->ifa_addr->sa_family != AF_INET)  continue;

                char ip[INET_ADDRSTRLEN]{};
                ::inet_ntop(AF_INET,
                    &reinterpret_cast<sockaddr_in*>(ifa->ifa_addr)->sin_addr,
                    ip, sizeof(ip));

                scopy(count == 0 ? reg.internalIp  : reg.internalIp2,
                      count == 0 ? sizeof(reg.internalIp) : sizeof(reg.internalIp2),
                      ip);
                ++count;
            }
            ::freeifaddrs(ifaddr);
        }
    }

    // mac address — first match across common interface names
    {
        static constexpr const char* kIfaces[] = {
            "/sys/class/net/eth0/address",
            "/sys/class/net/ens3/address",
            "/sys/class/net/wlan0/address",
        };
        for (const char* path : kIfaces) {
            std::ifstream f(path);
            std::string mac;
            if (f && std::getline(f, mac)) {
                scopy(reg.macAddress, sizeof(reg.macAddress), mac);
                break;
            }
        }
    }

    // default gateway — first zero-destination entry in /proc/net/route
    {
        std::ifstream f("/proc/net/route");
        std::string line;
        std::getline(f, line); // skip header
        while (std::getline(f, line)) {
            unsigned int dest = 0, gw = 0;
            char iface[16]{};
            if (std::sscanf(line.c_str(), "%15s %X %X", iface, &dest, &gw) == 3
                && dest == 0 && gw != 0) {
                in_addr addr{ .s_addr = gw };
                ::inet_ntop(AF_INET, &addr,
                            reg.defaultGateway, sizeof(reg.defaultGateway));
                break;
            }
        }
    }

    // dns — first nameserver line in /etc/resolv.conf
    {
        std::ifstream f("/etc/resolv.conf");
        for (std::string line; std::getline(f, line);) {
            if (line.starts_with("nameserver ")) {
                scopy(reg.dnsServer, sizeof(reg.dnsServer),
                      std::string_view(line).substr(11));
                break;
            }
        }
    }

    // proxy — standard environment variables
    {
        if (const char* proxy = env({"http_proxy", "HTTP_PROXY", "https_proxy"})) {
            reg.isProxy = true;
            scopy(reg.proxyUrl, sizeof(reg.proxyUrl), proxy);
        }
    }

    reg.isDomainJoined  = (reg.domain[0] != '\0');
    reg.selinuxEnabled  = (::access("/sys/fs/selinux/enforce",   F_OK) == 0);
    reg.apparmorEnabled = (::access("/sys/kernel/security/apparmor/profiles", F_OK) == 0);

    // timezone — prefer /etc/localtime symlink, fall back to /etc/timezone
    {
        char tzPath[256]{};
        ssize_t len = ::readlink("/etc/localtime", tzPath, sizeof(tzPath) - 1);
        if (len > 0) {
            tzPath[len] = '\0';
            if (const char* z = strstr(tzPath, "zoneinfo/"))
                scopy(reg.timezone, sizeof(reg.timezone), z + 9);
        } else {
            std::ifstream f("/etc/timezone");
            std::string tz;
            if (f && std::getline(f, tz))
                scopy(reg.timezone, sizeof(reg.timezone), tz);
        }
    }

    scopy(reg.locale, sizeof(reg.locale), env({"LANG", "LC_ALL"}));

    return reg;
}

} // namespace Raptor::Utils
