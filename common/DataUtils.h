#ifndef DATA_UTILS_H
#define DATA_UTILS_H

#include <vector>
#include <string>
#include <cstring>      // memcpy
#include <arpa/inet.h>  // htonl, ntohl
#include <iostream>

// ==========================================
// CLASS 1: PACKET BUILDER 
// ==========================================
class PacketBuilder {
private:
    std::vector<uint8_t> buffer;

public:
    PacketBuilder() {
        buffer.reserve(512); // Tối ưu bộ nhớ đệm ban đầu
    }

    // 1. Thêm số nguyên (4 bytes)
    void addInt(uint32_t value) {
        uint32_t netValue = htonl(value);
        const uint8_t* ptr = reinterpret_cast<const uint8_t*>(&netValue);
        buffer.insert(buffer.end(), ptr, ptr + sizeof(netValue));
    }

    // 2. Thêm chuỗi kí tự
    void addString(const std::string& str) {
        uint32_t len = static_cast<uint32_t>(str.length());
        addInt(len); // Tự động thêm độ dài trước
        buffer.insert(buffer.end(), str.begin(), str.end()); // Sau đó thêm nội dung
    }

    // 3. Thêm dữ liệu binary (ảnh, file...)
    void addBlob(const void* data, size_t size) {
        const uint8_t* ptr = static_cast<const uint8_t*>(data);
        buffer.insert(buffer.end(), ptr, ptr + size);
    }

    // Lấy buffer để gửi qua socket
    const std::vector<uint8_t>& getPacket() const { return buffer; }
    const void* getData() const { return buffer.data(); }
    size_t getSize() const { return buffer.size(); }

    void reset() { buffer.clear(); }
};

// ==========================================
// CLASS 2: PACKET READER 
// ==========================================
class PacketReader {
private:
    std::vector<uint8_t> buffer;
    size_t cursor; // Con trỏ tự động nhớ vị trí đang đọc

public:
    // Khởi tạo Reader từ dữ liệu nhận được
    PacketReader(const std::vector<uint8_t>& data) : buffer(data), cursor(0) {}
    
    // Constructor cho trường hợp nhận raw pointer từ socket
    PacketReader(const void* data, size_t size) : cursor(0) {
        const uint8_t* ptr = static_cast<const uint8_t*>(data);
        buffer.assign(ptr, ptr + size);
    }

    // 1. Đọc số nguyên
    uint32_t readInt() {
        if (cursor + 4 > buffer.size()) return 0; // Bảo vệ tràn bộ nhớ
        
        uint32_t netValue;
        std::memcpy(&netValue, &buffer[cursor], 4);
        cursor += 4;
        
        return ntohl(netValue);
    }

    // 2. Đọc chuỗi
    std::string readString() {
        // Bước 1: Đọc độ dài chuỗi (là 1 số int)
        uint32_t len = readInt(); 
        if (len == 0 && cursor >= buffer.size()) return ""; // Lỗi đọc length

        // Bước 2: Kiểm tra xem buffer còn đủ dữ liệu không
        if (cursor + len > buffer.size()) return "";

        // Bước 3: Trích xuất chuỗi
        std::string str(buffer.begin() + cursor, buffer.begin() + cursor + len);
        cursor += len;
        
        return str;
    }

    // Kiểm tra xem đã đọc hết chưa
    bool isFinished() const { return cursor >= buffer.size(); }
    
    // Lấy vị trí hiện tại (nếu cần debug)
    size_t getCursor() const { return cursor; }
};

#endif