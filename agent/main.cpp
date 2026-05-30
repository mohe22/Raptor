#include <arpa/inet.h>
#include <chrono>
#include <thread>
#include <unistd.h>
#include <vector>
#include <cstring>
#include <iostream>

#include "../common/header.hpp"

ssize_t send_all(int fd, const void* data, size_t size) {
    size_t totalSent = 0;
    const char* ptr = static_cast<const char*>(data);

    while (totalSent < size) {
        ssize_t sent = send(fd, ptr + totalSent, size - totalSent, 0);
        if (sent <= 0) return sent;
        totalSent += sent;
    }
    return totalSent;
}

ssize_t recv_all(int fd, void* buffer, size_t size) {
    size_t totalRecv = 0;
    char* ptr = static_cast<char*>(buffer);

    while (totalRecv < size) {
        ssize_t recvd = recv(fd, ptr + totalRecv, size - totalRecv, 0);
        if (recvd <= 0) return recvd;
        totalRecv += recvd;
    }
    return totalRecv;
}

int main() {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return 1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (connect(fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        return 1;
    }

    Raptor::Common::Header header;
    header.payloadSize = 1024 * 1024;
    header.setFlag(Raptor::Common::Flags::Ack | Raptor::Common::Flags::Binary);
    header.packetId = 1000;
    header.direction = Raptor::Common::PacketDirection::Req;
    header.type = Raptor::Common::PacketType::Command;

    auto serialized = header.serialize();

    if (send_all(fd, serialized.data(), serialized.size()) <= 0) {
        perror("send header");
        close(fd);
        return 1;
    }

    std::vector<char> payload(header.payloadSize, 'A');

    if (send_all(fd, payload.data(), payload.size()) <= 0) {
        perror("send payload");
        close(fd);
        return 1;
    }

    std::cout << "Sent payload, waiting for server response...\n";

    // Optional: wait for ACK or response
    char buf[256];
    ssize_t n = recv(fd, buf, sizeof(buf) - 1, 0);
    if (n > 0) {
        buf[n] = '\0';
        std::cout << "Server response: " << buf << std::endl;
    }

    // Keep connection alive a bit so server doesn't immediately drop it
    std::this_thread::sleep_for(std::chrono::seconds(5));

    close(fd);
}
