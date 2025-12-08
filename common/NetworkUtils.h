#ifndef NETWORK_UTILS_H
#define NETWORK_UTILS_H
#include "protocol.h"
#include <vector>
#include <cstdint>
#include <cstddef> // Cho size_t

class NetworkUtils {
public:
    // Hỗ trợ gửi toàn bộ dữ liệu (xử lý vòng lặp send cho đến khi hết data)
    static bool sendAll(int socket, const void* data, size_t len);

    // Hỗ trợ nhận toàn bộ dữ liệu (xử lý vòng lặp recv cho đến khi đủ data)
    static bool recvAll(int socket, void* buffer, size_t len);

    /**
     * Gửi một gói tin hoàn chỉnh theo giao thức:
     * [Opcode (1B)] + [PayloadLen (4B)] + [Payload (nếu có)]
     */
    static bool sendPacket(int socket, uint8_t opcode, const void* payload, uint32_t payloadLen);

    /**
     * Nhận một gói tin hoàn chỉnh.
     * Hàm này sẽ Block (chờ) cho đến khi nhận đủ Header và Payload.
     * @param outOpcode: Mã lệnh nhận được
     * @param outPayload: Dữ liệu nhận được (sẽ được resize tự động)
     */
    static bool recvPacket(int socket, uint8_t& outOpcode, std::vector<uint8_t>& outPayload);
};

#endif // NETWORK_UTILS_H