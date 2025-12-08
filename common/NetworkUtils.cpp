#include "NetworkUtils.h"
#include "protocol.h" 

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring> 
#include <iostream>
#include <vector>

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
    // 1. Chuẩn bị Header
    MessageHeader header;
    
    // Xóa sạch bộ nhớ của header để tránh rác (Best Practice)
    std::memset(&header, 0, sizeof(header)); 
    
    header.opcode = opcode;
    header.payloadLen = htonl(payloadLen); // Chuyển sang Big Endian

    // 2. Gửi Header (5 bytes)
    if (!sendAll(socket, &header, sizeof(header))) return false;

    // 3. Gửi Payload (nếu có)
    if (payloadLen > 0 && payload != nullptr) {
        if (!sendAll(socket, payload, payloadLen)) return false;
    }
    return true;
}

bool NetworkUtils::recvPacket(int socket, uint8_t& outOpcode, std::vector<uint8_t>& outPayload) {
    // 1. Nhận Header
    // Nhận vào buffer tạm trước
    uint8_t headerBuf[sizeof(MessageHeader)]; 
    if (!recvAll(socket, headerBuf, sizeof(MessageHeader))) return false;

    // 2. Parse Header an toàn bằng memcpy 
    MessageHeader header;
    std::memcpy(&header, headerBuf, sizeof(MessageHeader));

    outOpcode = header.opcode;
    uint32_t len = ntohl(header.payloadLen); // Chuyển từ Big Endian về máy

    // 3. Nhận Payload
    outPayload.clear();
    if (len > 0) {
        // Kiểm tra kích thước gói tin tối đa để tránh bị tấn công tràn RAM (10 MB)
        if (len > 10 * 1024 * 1024) {
            std::cerr << "Error: Packet too large (" << len << " bytes)" << std::endl;
            return false;
        }

        outPayload.resize(len);
        // Nhận dữ liệu thẳng vào vector
        if (!recvAll(socket, outPayload.data(), len)) return false;
    }
    return true;
}