// FILE: common/protocol.h
#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <cstdint>

// ====================================================
// 1. CẤU TRÚC GÓI TIN (PACKET STRUCTURE) [cite: 23, 24]
// ====================================================
// Sử dụng #pragma pack(1) để đảm bảo không có padding byte,
// giúp kích thước struct chính xác là 5 bytes khi gửi qua mạng.
#pragma pack(push, 1)
struct MessageHeader {
    uint8_t opcode;       // Mã lệnh (1 Byte)
    uint32_t payloadLen;  // Độ dài dữ liệu đi kèm (4 Bytes) - Cần dùng htonl/ntohl
};
#pragma pack(pop)

// ====================================================
// 2. BẢNG MÃ LỆNH (OPCODE) [cite: 76]
// ====================================================
enum OpCode : uint8_t {
    // A. Nhóm Quản lý Tài khoản (Account) [cite: 30, 31]
    REQ_REGISTER        = 1,
    RES_REGISTER        = 2,
    REQ_LOGIN           = 3,
    RES_LOGIN           = 4,
    REQ_LOGOUT          = 5,

    // B. Nhóm Quản lý Topic (Producer) [cite: 32, 33]
    REQ_GET_ALL_TOPICS  = 6,
    RES_GET_ALL_TOPICS  = 7,
    REQ_CREATE_TOPIC    = 8,
    RES_CREATE_TOPIC    = 9,
    REQ_DELETE_TOPIC    = 10,
    RES_DELETE_TOPIC    = 11,
    REQ_GET_MY_TOPICS   = 12,
    RES_GET_MY_TOPICS   = 13,
    NOTIFY_TOPIC_EVENT  = 14, // Broadcast: Có topic mới/xóa

    // C. Nhóm Tra cứu & Theo dõi (Consumer) [cite: 34, 35]
    REQ_TOPIC_INFO      = 15,
    RES_TOPIC_INFO      = 16,
    REQ_TOPIC_SUBS      = 17, // Xem ai đang sub topic này
    RES_TOPIC_SUBS      = 18,
    REQ_SUBSCRIBE       = 19,
    RES_SUBSCRIBE       = 20,
    REQ_UNSUBSCRIBE     = 21,
    RES_UNSUBSCRIBE     = 22,
    REQ_MY_SUBS         = 23, // Xem mình đang sub gì
    RES_MY_SUBS         = 24,

    // D. Nhóm Tin nhắn & Dữ liệu (Data) [cite: 36, 37]
    REQ_PUBLISH_TEXT    = 25,
    REQ_PUBLISH_BIN     = 26, // Gửi ảnh/âm thanh
    RES_PUBLISH         = 27, // Server xác nhận đã nhận tin
    NOTIFY_MSG_TEXT     = 28, // Server đẩy tin nhắn cho người sub
    NOTIFY_MSG_BIN      = 29, // Server đẩy file cho người sub
    REQ_HISTORY         = 30,
    RES_HISTORY_START   = 31,
    RES_HISTORY_ITEM    = 32,
    RES_HISTORY_END     = 33
};

// ====================================================
// 3. MÃ TRẠNG THÁI (STATUS CODE) [cite: 76]
// ====================================================
// Dùng cho payload của các gói RES_...
enum StatusCode : uint8_t {
    // Chung
    STATUS_SUCCESS          = 0,
    STATUS_FAIL             = 1,

    // Register / Login
    REGISTER_OK             = 0,
    REGISTER_FAIL_EXISTS    = 1,
    REGISTER_FAIL_INVALID   = 2,
    
    LOGIN_OK                = 0,
    LOGIN_FAIL_NOT_FOUND    = 1,
    LOGIN_FAIL_WRONG_PASS   = 2,
    LOGIN_FAIL_ALREADY_LOGGED = 3,

    // Topic
    TOPIC_CREATE_OK         = 0,
    TOPIC_FAIL_EXISTS       = 1,
    TOPIC_FAIL_INVALID      = 2,
    
    TOPIC_DELETE_OK         = 0,
    TOPIC_FAIL_NOT_FOUND    = 1,
    TOPIC_FAIL_DENIED       = 2, // Không phải chủ topic

    // Subscribe
    SUB_OK                  = 0,
    SUB_FAIL_NOT_FOUND      = 1,
    SUB_FAIL_ALREADY        = 2,

    UNSUB_OK                = 0,
    UNSUB_FAIL_NOT_YET      = 1
};

// ====================================================
// 4. CONSTANTS [cite: 27, 28]
// ====================================================
// Định nghĩa các hằng số kích thước nếu cần
#define HEADER_SIZE sizeof(MessageHeader) // 5 bytes

#endif // PROTOCOL_H