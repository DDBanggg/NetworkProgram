#ifndef DATA_UTILS_H
#define DATA_UTILS_H

#include <vector>
#include <string>
#include <cstring>
#include <arpa/inet.h> // Cho htonl, ntohl

class DataUtils {
public:
    // Hỗ trợ đóng gói chuỗi (String) -> Byte Vector
    // Output: [Length (4B Big Endian)] + [String Data]
    static std::vector<uint8_t> packString(const std::string& str) {
        std::vector<uint8_t> buffer;
        uint32_t len = str.length();
        uint32_t netLen = htonl(len); // Chuyển sang Big Endian

        // Chèn độ dài (4 bytes)
        const uint8_t* lenPtr = reinterpret_cast<const uint8_t*>(&netLen);
        buffer.insert(buffer.end(), lenPtr, lenPtr + sizeof(netLen));

        // Chèn dữ liệu chuỗi
        buffer.insert(buffer.end(), str.begin(), str.end());
        
        return buffer;
    }

    // Hỗ trợ đóng gói số nguyên (Int) -> Byte Vector
    static std::vector<uint8_t> packInt(uint32_t value) {
        std::vector<uint8_t> buffer;
        uint32_t netValue = htonl(value);
        const uint8_t* ptr = reinterpret_cast<const uint8_t*>(&netValue);
        buffer.insert(buffer.end(), ptr, ptr + sizeof(netValue));
        return buffer;
    }

    // Hỗ trợ giải nén chuỗi từ buffer
    // offset: Vị trí bắt đầu đọc trong buffer (sẽ tự động tăng lên sau khi đọc)
    static std::string unpackString(const std::vector<uint8_t>& buffer, size_t& offset) {
        if (offset + 4 > buffer.size()) return ""; // Không đủ byte độ dài

        // 1. Đọc độ dài
        uint32_t netLen;
        std::memcpy(&netLen, &buffer[offset], 4);
        uint32_t len = ntohl(netLen);
        offset += 4;

        if (offset + len > buffer.size()) return ""; // Không đủ byte dữ liệu

        // 2. Đọc chuỗi
        std::string str(buffer.begin() + offset, buffer.begin() + offset + len);
        offset += len;

        return str;
    }
};

#endif
```json