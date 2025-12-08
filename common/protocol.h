#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <cstdint>

// ====================================================
// 1. CẤU TRÚC GÓI TIN (PACKET STRUCTURE) 
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
// 2. BẢNG MÃ LỆNH (OPCODE) 
// ====================================================
enum OpCode : uint8_t {
    // A. Nhóm Quản lý Tài khoản (Account) 
    REQ_REGISTER        = 1,  // [C->S] Đăng ký tài khoản. Payload: [user_len][user][pass_len][pass]
    RES_REGISTER        = 2,  // [S->C] Trả về kết quả ĐK. Payload: [status_code(1B)]
    REQ_LOGIN           = 3,  // [C->S] Đăng nhập. Payload: [user_len][user][pass_len][pass]
    RES_LOGIN           = 4,  // [S->C] Trả về kết quả ĐN. Payload: [status_code(1B)]
    REQ_LOGOUT          = 5,  // [C->S] Ngắt kết nối an toàn. Payload: (Rỗng)

    // B. Nhóm Quản lý Topic (Producer) 
    REQ_GET_ALL_TOPICS  = 6,  // [C->S] Lấy list topic. Payload: (Rỗng)
    RES_GET_ALL_TOPICS  = 7,  // [S->C] Trả về list. Payload: [count] + List<Topic>
    REQ_CREATE_TOPIC    = 8,  // [C->S] Tạo topic mới. Payload: [name_len][name]
    RES_CREATE_TOPIC    = 9,  // [S->C] Trả ID topic mới. Payload: [status][topic_id]
    REQ_DELETE_TOPIC    = 10, // [C->S] Xóa topic. Payload: [topic_id]
    RES_DELETE_TOPIC    = 11, // [S->C] KQ xóa. Payload: [status]
    REQ_GET_MY_TOPICS   = 12, // [C->S] Lấy topic mình tạo. Payload: (Rỗng)
    RES_GET_MY_TOPICS   = 13, // [S->C] Trả về list. Payload: [count] + List<Topic>
    NOTIFY_TOPIC_EVENT  = 14, // [S->All] Broadcast: Có topic mới/xóa. Payload: [action][id][name_len][name]

    // C. Nhóm Tra cứu & Theo dõi (Consumer) 
    REQ_TOPIC_INFO      = 15, // [C->S] Xem tác giả, sub count. Payload: [topic_id]
    RES_TOPIC_INFO      = 16, // [S->C] Trả thông tin. Payload: [auth_len][auth][sub_cnt]
    REQ_TOPIC_SUBS      = 17, // [C->S] Xem ai đang sub topic này. Payload: [topic_id]
    RES_TOPIC_SUBS      = 18, // [S->C] Trả list username. Payload: [count] + List<User>
    REQ_SUBSCRIBE       = 19, // [C->S] Đăng ký nhận tin. Payload: [topic_id]
    RES_SUBSCRIBE       = 20, // [S->C] KQ đăng ký. Payload: [status]
    REQ_UNSUBSCRIBE     = 21, // [C->S] Hủy đăng ký. Payload: [topic_id]
    RES_UNSUBSCRIBE     = 22, // [S->C] KQ hủy. Payload: [status]
    REQ_MY_SUBS         = 23, // [C->S] Xem mình đang sub gì. Payload: (Rỗng)
    RES_MY_SUBS         = 24, // [S->C] Trả list topic đã sub. Payload: [count] + List<Topic>

    // D. Nhóm Tin nhắn & Dữ liệu (Data) 
    REQ_PUBLISH_TEXT    = 25, // [C->S] Gửi text. Payload: [topic_id][msg_len][msg]
    REQ_PUBLISH_BIN     = 26, // [C->S] Gửi Ảnh/Audio. Payload: [topic_id][ext_len][ext][len][data]
    RES_PUBLISH         = 27, // [S->C] Server xác nhận đã nhận tin. Payload: [status]
    NOTIFY_MSG_TEXT     = 28, // [S->C] Push tin về Sub. Payload: [topic_id][u_len][user][len][msg]
    NOTIFY_MSG_BIN      = 29, // [S->C] Push file về Sub. Payload: [topic_id][u_len][user][ext...][data]
    REQ_HISTORY         = 30, // [C->S] Lấy tin cũ. Payload: [topic_id]
    RES_HISTORY_START   = 31, // [S->C] Báo số lượng tin. Payload: [msg_count]
    RES_HISTORY_ITEM    = 32, // [S->C] Gửi từng tin cũ. Payload: [type(1B)] + [Payload]
    RES_HISTORY_END     = 33  // [S->C] Kết thúc history. Payload: (Rỗng)
};

// ====================================================
// 3. MÃ TRẠNG THÁI (STATUS CODE)
// ====================================================
// Dùng cho payload của các gói RES_...
enum StatusCode : uint8_t {
    // Chung
    STATUS_SUCCESS          = 0, // Thành công chung
    STATUS_FAIL             = 1, // Thất bại chung

    // Register / Login
    REGISTER_OK             = 0, // Đăng ký thành công
    REGISTER_FAIL_EXISTS    = 1, // Tên đăng nhập đã tồn tại
    REGISTER_FAIL_INVALID   = 2, // Tên đăng nhập/mật khẩu không hợp lệ
    
    LOGIN_OK                = 0, // Đăng nhập thành công
    LOGIN_FAIL_NOT_FOUND    = 1, // Tài khoản không tồn tại
    LOGIN_FAIL_WRONG_PASS   = 2, // Sai mật khẩu
    LOGIN_FAIL_ALREADY_LOGGED = 3, // Tài khoản đã đăng nhập ở nơi khác

    // Topic
    TOPIC_CREATE_OK         = 0, // Tạo topic thành công
    TOPIC_FAIL_EXISTS       = 1, // Topic đã tồn tại
    TOPIC_FAIL_INVALID      = 2, // Tên topic không hợp lệ
    
    TOPIC_DELETE_OK         = 0, // Xóa topic thành công
    TOPIC_FAIL_NOT_FOUND    = 1, // Topic không tìm thấy
    TOPIC_FAIL_DENIED       = 2, // Không có quyền xóa topic này

    // Subscribe
    SUB_OK                  = 0, // Đăng ký theo dõi thành công
    SUB_FAIL_NOT_FOUND      = 1, // Topic không tìm thấy
    SUB_FAIL_ALREADY        = 2, // Đã đăng ký theo dõi rồi

    UNSUB_OK                = 0, // Hủy đăng ký theo dõi thành công
    UNSUB_FAIL_NOT_YET      = 1  // Chưa đăng ký theo dõi nên không thể hủy
};

// ====================================================
// 4. CONSTANTS 
// ====================================================
// Định nghĩa các hằng số kích thước nếu cần
#define HEADER_SIZE sizeof(MessageHeader) // 5 bytes

#endif // PROTOCOL_H