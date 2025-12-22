#include "NetworkUtils.h"
#include "protocol.h" 

#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring> 
#include <iostream>
#include <vector>
#include <iomanip> // Thư viện để in Hex

using namespace std;

void logRawHex(const string& label, const void* data, size_t len) {
    const uint8_t* ptr = static_cast<const uint8_t*>(data);
    cout << "[" << label << "] (" << len << " bytes): ";
    for (size_t i = 0; i < len; ++i) {
        // In ra 2 ký tự Hex, có số 0 đằng trước nếu cần
        cout << hex << setfill('0') << setw(2) << (int)ptr[i] << " ";
    }
    cout << dec << endl; // Reset về hệ thập phân
}

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
    std::memset(&header, 0, sizeof(header)); 
    header.opcode = opcode;
    header.payloadLen = htonl(payloadLen);

    // 2. Tạo Buffer chứa cả Header + Payload
    std::vector<uint8_t> packetBuffer;
    packetBuffer.reserve(sizeof(MessageHeader) + payloadLen);

    // Chèn Header
    const uint8_t* headerPtr = reinterpret_cast<const uint8_t*>(&header);
    packetBuffer.insert(packetBuffer.end(), headerPtr, headerPtr + sizeof(header));

    // Chèn Payload 
    if (payloadLen > 0 && payload != nullptr) {
        const uint8_t* payloadPtr = static_cast<const uint8_t*>(payload);
        packetBuffer.insert(packetBuffer.end(), payloadPtr, payloadPtr + payloadLen);
    }

    // 3. Gửi 
    return sendAll(socket, packetBuffer.data(), packetBuffer.size());
}

bool NetworkUtils::recvPacket(int socket, uint8_t& outOpcode, std::vector<uint8_t>& outPayload) {
    // 1. Nhận Header (5 bytes)
    uint8_t headerBuf[sizeof(MessageHeader)]; 
    if (!recvAll(socket, headerBuf, sizeof(MessageHeader))) return false;

    // Parse Header để biết độ dài payload cần đọc tiếp
    MessageHeader header;
    std::memcpy(&header, headerBuf, sizeof(MessageHeader));

    outOpcode = header.opcode;
    uint32_t len = ntohl(header.payloadLen);

    // 2. Nhận Payload
    outPayload.clear();
    if (len > 0) {
        // Kiểm tra an toàn bộ nhớ (Max 10MB)
        if (len > 10 * 1024 * 1024) {
            std::cerr << "Error: Packet too large (" << len << " bytes)" << std::endl;
            return false;
        }

        outPayload.resize(len);
        if (!recvAll(socket, outPayload.data(), len)) return false;
    }

    std::vector<uint8_t> fullLogBuffer;
    fullLogBuffer.reserve(sizeof(MessageHeader) + len);
    
    // Thêm header vào log buffer
    fullLogBuffer.insert(fullLogBuffer.end(), headerBuf, headerBuf + sizeof(MessageHeader));
    
    // Thêm payload vào log buffer
    if (len > 0) {
        fullLogBuffer.insert(fullLogBuffer.end(), outPayload.begin(), outPayload.end());
    }

    logRawHex("RECV PACKET", fullLogBuffer.data(), fullLogBuffer.size());

    return true;
}