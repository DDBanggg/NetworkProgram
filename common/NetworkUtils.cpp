#include "NetworkUtils.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <iostream>

bool NetworkUtils::sendAll(int socket, const void* data, size_t len) {
    const char* ptr = static_cast<const char*>(data);
    size_t totalSent = 0;
    while (totalSent < len) {
        ssize_t sent = send(socket, ptr + totalSent, len - totalSent, 0);
        if (sent <= 0) return false;
        totalSent += sent;
    }
    return true;
}

bool NetworkUtils::recvAll(int socket, void* buffer, size_t len) {
    char* ptr = static_cast<char*>(buffer);
    size_t totalReceived = 0;
    while (totalReceived < len) {
        ssize_t received = recv(socket, ptr + totalReceived, len - totalReceived, 0);
        if (received <= 0) return false;
        totalReceived += received;
    }
    return true;
}

bool NetworkUtils::sendPacket(int socket, uint8_t opcode, const void* payload, uint32_t payloadLen) {
    MessageHeader header;
    header.opcode = opcode;
    header.payloadLen = htonl(payloadLen);

    // Gửi Header
    if (!sendAll(socket, &header, sizeof(header))) return false;

    // Gửi Payload (nếu có)
    if (payloadLen > 0 && payload != nullptr) {
        if (!sendAll(socket, payload, payloadLen)) return false;
    }
    return true;
}

bool NetworkUtils::recvPacket(int socket, uint8_t& outOpcode, std::vector<uint8_t>& outPayload) {
    MessageHeader header;
    if (!recvAll(socket, &header, sizeof(header))) return false;

    outOpcode = header.opcode;
    uint32_t len = ntohl(header.payloadLen);

    outPayload.clear();
    if (len > 0) {
        outPayload.resize(len);
        if (!recvAll(socket, outPayload.data(), len)) return false;
    }
    return true;
}