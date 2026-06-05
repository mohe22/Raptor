#include <arpa/inet.h>
#include <sys/statvfs.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <ctime>
#include <sys/utsname.h>
#include <sys/sysinfo.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <fstream>
#include <chrono>
#include<thread>
#include <string>
#include <print>

#include "../common/header.hpp"
#include "../common/register.hpp"

Raptor::Common::Register collect() noexcept {
    Raptor::Common::Register reg{};

    gethostname(reg.hostname, sizeof(reg.hostname));

    const char* user = getenv("USER");
    if (!user) user = getenv("LOGNAME");
    if (user) strncpy(reg.username, user, sizeof(reg.username) - 1);

    const char* home = getenv("HOME");
    if (home) strncpy(reg.homeDir, home, sizeof(reg.homeDir) - 1);

    const char* shell = getenv("SHELL");
    if (shell) strncpy(reg.shell, shell, sizeof(reg.shell) - 1);

    reg.isAdmin = (geteuid() == 0);

    // sudoer check
    {
        std::ifstream f("/etc/group");
        std::string line;
        while (std::getline(f, line)) {
            if (line.find("sudo:") == 0 || line.find("wheel:") == 0) {
                if (line.find(reg.username) != std::string::npos) {
                    reg.isSudoer = true;
                    break;
                }
            }
        }
    }

    // domain
    {
        std::ifstream f("/proc/sys/kernel/domainname");
        if (f) {
            std::string line;
            std::getline(f, line);
            if (line != "(none)")
                strncpy(reg.domain, line.c_str(), sizeof(reg.domain) - 1);
        }
    }

    // os / arch / kernel
    {
        struct utsname u{};
        if (uname(&u) == 0) {
            strncpy(reg.os,            u.sysname, sizeof(reg.os)            - 1);
            strncpy(reg.osVersion,     u.release, sizeof(reg.osVersion)     - 1);
            strncpy(reg.kernelVersion, u.version, sizeof(reg.kernelVersion) - 1);
            strncpy(reg.arch,          u.machine, sizeof(reg.arch)          - 1);
        }
    }

    // cpu name
    {
        std::ifstream f("/proc/cpuinfo");
        std::string line;
        while (std::getline(f, line)) {
            if (line.find("model name") != std::string::npos) {
                auto pos = line.find(':');
                if (pos != std::string::npos)
                    strncpy(reg.cpu, line.substr(pos + 2).c_str(), sizeof(reg.cpu) - 1);
                break;
            }
        }
    }

    reg.cpuCores = static_cast<uint32_t>(sysconf(_SC_NPROCESSORS_ONLN));

    // ram
    {
        struct sysinfo si{};
        if (sysinfo(&si) == 0) {
            reg.ramBytes    = si.totalram  * si.mem_unit;
            reg.uptimeSeconds = si.uptime;
        }
    }

    // disk
    {
        struct statvfs sv{};
        if (statvfs("/", &sv) == 0) {
            reg.diskTotalBytes = sv.f_blocks * sv.f_frsize;
            reg.diskFreeBytes  = sv.f_bfree  * sv.f_frsize;
        }
    }

    reg.pid = static_cast<uint32_t>(getpid());

    // process path
    {
        ssize_t len = readlink("/proc/self/exe", reg.processPath, sizeof(reg.processPath) - 1);
        if (len > 0) {
            reg.processPath[len] = '\0';
            const char* slash = strrchr(reg.processPath, '/');
            strncpy(reg.processName, slash ? slash + 1 : reg.processPath, sizeof(reg.processName) - 1);
        }
    }

    // docker
    reg.isDocker = (access("/.dockerenv", F_OK) == 0);

    // virtualization
    {
        std::ifstream f("/sys/class/dmi/id/product_name");
        if (f) {
            std::string name;
            std::getline(f, name);
            if (name.find("VMware")     != std::string::npos) { reg.isVM = true; strncpy(reg.vmType, "VMware",     sizeof(reg.vmType) - 1); }
            else if (name.find("VirtualBox") != std::string::npos) { reg.isVM = true; strncpy(reg.vmType, "VirtualBox", sizeof(reg.vmType) - 1); }
            else if (name.find("KVM")   != std::string::npos) { reg.isVM = true; strncpy(reg.vmType, "KVM",        sizeof(reg.vmType) - 1); }
            else if (name.find("QEMU")  != std::string::npos) { reg.isVM = true; strncpy(reg.vmType, "QEMU",       sizeof(reg.vmType) - 1); }
        }
        // also check cpuinfo hypervisor flag
        if (!reg.isVM) {
            std::ifstream cf("/proc/cpuinfo");
            std::string line;
            while (std::getline(cf, line)) {
                if (line.find("hypervisor") != std::string::npos) {
                    reg.isVM = true;
                    if (reg.vmType[0] == '\0')
                        strncpy(reg.vmType, "Unknown", sizeof(reg.vmType) - 1);
                    break;
                }
            }
        }
    }

    // network interfaces
    {
        struct ifaddrs* ifaddr = nullptr;
        if (getifaddrs(&ifaddr) == 0) {
            int ipCount = 0;
            for (auto* ifa = ifaddr; ifa; ifa = ifa->ifa_next) {
                if (!ifa->ifa_addr) continue;
                if (ifa->ifa_flags & IFF_LOOPBACK) continue;
                if (ifa->ifa_addr->sa_family == AF_INET) {
                    char ip[INET_ADDRSTRLEN]{};
                    inet_ntop(AF_INET,
                        &reinterpret_cast<sockaddr_in*>(ifa->ifa_addr)->sin_addr,
                        ip, sizeof(ip));
                    if (ipCount == 0)      strncpy(reg.internalIp,  ip, sizeof(reg.internalIp)  - 1);
                    else if (ipCount == 1) strncpy(reg.internalIp2, ip, sizeof(reg.internalIp2) - 1);
                    ipCount++;
                }
            }
            freeifaddrs(ifaddr);
        }
    }

    // mac address
    {
        std::ifstream f("/sys/class/net/eth0/address");
        if (!f) f.open("/sys/class/net/ens3/address");
        if (!f) f.open("/sys/class/net/wlan0/address");
        if (f) {
            std::string mac;
            std::getline(f, mac);
            strncpy(reg.macAddress, mac.c_str(), sizeof(reg.macAddress) - 1);
        }
    }

    // default gateway
    {
        std::ifstream f("/proc/net/route");
        std::string line;
        std::getline(f, line); // skip header
        while (std::getline(f, line)) {
            unsigned int dest, gw;
            char iface[16]{};
            if (sscanf(line.c_str(), "%15s %X %X", iface, &dest, &gw) == 3 && dest == 0 && gw != 0) {
                struct in_addr addr{};
                addr.s_addr = gw;
                inet_ntop(AF_INET, &addr, reg.defaultGateway, sizeof(reg.defaultGateway));
                break;
            }
        }
    }

    // dns
    {
        std::ifstream f("/etc/resolv.conf");
        std::string line;
        while (std::getline(f, line)) {
            if (line.find("nameserver") == 0) {
                auto pos = line.find(' ');
                if (pos != std::string::npos)
                    strncpy(reg.dnsServer, line.substr(pos + 1).c_str(), sizeof(reg.dnsServer) - 1);
                break;
            }
        }
    }

    // proxy
    {
        const char* proxy = getenv("http_proxy");
        if (!proxy) proxy = getenv("HTTP_PROXY");
        if (!proxy) proxy = getenv("https_proxy");
        if (proxy && proxy[0] != '\0') {
            reg.isProxy = true;
            strncpy(reg.proxyUrl, proxy, sizeof(reg.proxyUrl) - 1);
        }
    }

    reg.isDomainJoined = (reg.domain[0] != '\0');

    // selinux
    reg.selinuxEnabled  = (access("/sys/fs/selinux/enforce", F_OK) == 0);

    // apparmor
    reg.apparmorEnabled = (access("/sys/kernel/security/apparmor/profiles", F_OK) == 0);

    // timezone
    {
        char tzPath[256]{};
        ssize_t len = readlink("/etc/localtime", tzPath, sizeof(tzPath) - 1);
        if (len > 0) {
            tzPath[len] = '\0';
            const char* zoneinfo = strstr(tzPath, "zoneinfo/");
            if (zoneinfo)
                strncpy(reg.timezone, zoneinfo + 9, sizeof(reg.timezone) - 1);
        } else {
            std::ifstream f("/etc/timezone");
            if (f) {
                std::string tz;
                std::getline(f, tz);
                strncpy(reg.timezone, tz.c_str(), sizeof(reg.timezone) - 1);
            }
        }
    }

    // locale
    {
        const char* lang = getenv("LANG");
        if (!lang) lang = getenv("LC_ALL");
        if (lang) strncpy(reg.locale, lang, sizeof(reg.locale) - 1);
    }


    return reg;
}

void printRegister(const Raptor::Common::Register& reg) {
    std::println("=== Raptor System Register ===");
    std::println("Hostname          : {}", reg.hostname);
    std::println("Username          : {}", reg.username);
    std::println("Home Dir          : {}", reg.homeDir);
    std::println("Shell             : {}", reg.shell);
    std::println("Is Admin          : {}", reg.isAdmin);
    std::println("Is Sudoer         : {}", reg.isSudoer);
    std::println("Domain            : {}", reg.domain);
    std::println("OS                : {} {}", reg.os, reg.osVersion);
    std::println("Kernel            : {}", reg.kernelVersion);
    std::println("Arch              : {}", reg.arch);
    std::println("CPU               : {}", reg.cpu);
    std::println("CPU Cores         : {}", reg.cpuCores);
    std::println("RAM               : {} bytes", reg.ramBytes);
    std::println("Disk Total        : {} bytes", reg.diskTotalBytes);
    std::println("Disk Free         : {} bytes", reg.diskFreeBytes);
    std::println("Uptime (sec)      : {}", reg.uptimeSeconds);
    std::println("PID               : {}", reg.pid);
    std::println("Process Path      : {}", reg.processPath);
    std::println("Process Name      : {}", reg.processName);
    std::println("Is Docker         : {}", reg.isDocker);
    std::println("Is VM             : {} ({})", reg.isVM, reg.vmType);
    std::println("Internal IP       : {}", reg.internalIp);
    std::println("Internal IP2      : {}", reg.internalIp2);
    std::println("MAC Address       : {}", reg.macAddress);
    std::println("Default Gateway   : {}", reg.defaultGateway);
    std::println("DNS Server        : {}", reg.dnsServer);
    std::println("Is Proxy          : {}", reg.isProxy);
    std::println("Proxy URL         : {}", reg.proxyUrl);
    std::println("Domain Joined     : {}", reg.isDomainJoined);
    std::println("SELinux Enabled   : {}", reg.selinuxEnabled);
    std::println("AppArmor Enabled  : {}", reg.apparmorEnabled);
    std::println("Timezone          : {}", reg.timezone);
    std::println("Locale            : {}", reg.locale);
    std::println("==============================");
}

ssize_t send_all(int fd, const void* data, size_t size) {
    size_t sent = 0;
    const char* ptr = static_cast<const char*>(data);
    while (sent < size) {
        ssize_t n = send(fd, ptr + sent, size - sent, 0);
        if (n <= 0) return n;
        sent += n;
    }
    return sent;
}

ssize_t recv_all(int fd, void* buffer, size_t size) {
    size_t recvd = 0;
    char* ptr = static_cast<char*>(buffer);
    while (recvd < size) {
        ssize_t n = recv(fd, ptr + recvd, size - recvd, 0);
        if (n <= 0) return n;
        recvd += n;
    }
    return recvd;
}

int main() {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); return 1; }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (connect(fd, (sockaddr*)&addr, sizeof(addr)) < 0) { perror("connect"); return 1; }

    auto reg = collect();
    printRegister(reg);

    auto payload = reg.serialize();

    Raptor::Common::Header header;
    header.payloadSize = static_cast<uint32_t>(payload.size());
    header.setFlag(Raptor::Common::Flags::Ack | Raptor::Common::Flags::Binary);
    header.packetId   = 1000;
    header.direction  = Raptor::Common::PacketDirection::Req;
    header.type  = Raptor::Common::PacketType::Register;

    auto headerBytes = header.serialize();

    if (send_all(fd, headerBytes.data(), headerBytes.size()) <= 0) { perror("send header"); close(fd); return 1; }
    if (send_all(fd, payload.data(),     payload.size())     <= 0) { perror("send payload"); close(fd); return 1; }

    std::println("Register packet sent ({} bytes)", payload.size());

    std::this_thread::sleep_for(std::chrono::seconds(120));


    close(fd);
}
