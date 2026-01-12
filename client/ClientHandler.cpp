#include "ClientHandler.h"
#include "../common/NetworkUtils.h"
#include "../common/protocol.h"
#include "../common/DataUtils.h"
#include <iostream>
#include <fstream>
#include <vector>

using namespace std;

ClientHandler::ClientHandler(int socket) : serverSocket(socket) {}

// =======================================================
// NHÓM 1: ĐĂNG KÝ & ĐĂNG NHẬP (Blocking Mode)
// =======================================================

void ClientHandler::requestRegister(const string& user, const string& pass) {
    PacketBuilder builder;
    builder.addString(user);
    builder.addString(pass);

    if (!NetworkUtils::sendPacket(serverSocket, REQ_REGISTER, builder.getData(), builder.getSize())) {
        cout << "Loi gui yeu cau dang ky!" << endl;
        return;
    }

    // Chờ phản hồi ngay lập tức
    uint8_t opcode;
    vector<uint8_t> payload;
    if (NetworkUtils::recvPacket(serverSocket, opcode, payload) && opcode == RES_REGISTER) {
        if (!payload.empty() && payload[0] == REG_SUCCESS) {
            cout << ">> Dang ky thanh cong!" << endl;
        } else {
            cout << ">> Dang ky THAT BAI (Tai khoan da ton tai)." << endl;
        }
    }
}

bool ClientHandler::requestLogin(const string& user, const string& pass) {
    PacketBuilder builder;
    builder.addString(user);
    builder.addString(pass);

    if (!NetworkUtils::sendPacket(serverSocket, REQ_LOGIN, builder.getData(), builder.getSize())) {
        cout << "Loi gui yeu cau dang nhap!" << endl;
        return false;
    }

    // Chờ phản hồi ngay lập tức
    uint8_t opcode;
    vector<uint8_t> payload;
    if (NetworkUtils::recvPacket(serverSocket, opcode, payload) && opcode == RES_LOGIN) {
        if (!payload.empty() && payload[0] == LOGIN_SUCCESS) {
            cout << ">> Dang nhap thanh cong!" << endl;
            return true;
        } else {
            cout << ">> Dang nhap THAT BAI (Sai thong tin hoac da dang nhap)." << endl;
        }
    }
    return false;
}

// =======================================================
// NHÓM CÁC HÀM SAU CHỈ GỬI (Non-Blocking)
// Phản hồi sẽ được xử lý bởi listenerThread trong ClientApp
// =======================================================

void ClientHandler::requestGetList(bool myTopics) {
    // Nếu myTopics = true -> Lấy topic của tôi, ngược lại lấy tất cả
    uint8_t op = myTopics ? REQ_GET_MY_TOPICS : REQ_GET_ALL_TOPICS;
    // Payload rỗng
    NetworkUtils::sendPacket(serverSocket, op, nullptr, 0);
}

void ClientHandler::requestCreateTopic(const string& name, const string& desc) {
    PacketBuilder builder;
    builder.addString(name);
    builder.addString(desc); // Đã thêm Description
    NetworkUtils::sendPacket(serverSocket, REQ_CREATE_TOPIC, builder.getData(), builder.getSize());
    cout << "[INFO] Da gui yeu cau tao Topic..." << endl;
}

void ClientHandler::requestDeleteTopic(uint32_t topicId) {
    PacketBuilder builder;
    builder.addInt(topicId); // Gửi ID dạng Int
    NetworkUtils::sendPacket(serverSocket, REQ_DELETE_TOPIC, builder.getData(), builder.getSize());
}

void ClientHandler::requestSubscribe(uint32_t topicId) {
    PacketBuilder builder;
    builder.addInt(topicId);
    NetworkUtils::sendPacket(serverSocket, REQ_SUBSCRIBE, builder.getData(), builder.getSize());
}

void ClientHandler::requestUnsubscribe(uint32_t topicId) {
    PacketBuilder builder;
    builder.addInt(topicId);
    NetworkUtils::sendPacket(serverSocket, REQ_UNSUBSCRIBE, builder.getData(), builder.getSize());
}

void ClientHandler::requestPublish(uint32_t topicId, const string& message) {
    PacketBuilder builder;
    builder.addInt(topicId);
    builder.addString(message);
    NetworkUtils::sendPacket(serverSocket, REQ_PUBLISH_TEXT, builder.getData(), builder.getSize());
}

// --- XỬ LÝ GỬI FILE (SPRINT 2) ---
void ClientHandler::requestPublishBinary(uint32_t topicId, const string& filePath) {
    // 1. Mở file chế độ Binary
    ifstream file(filePath, ios::binary | ios::ate); // ios::ate để con trỏ ở cuối -> lấy size
    if (!file.is_open()) {
        cout << "[ERROR] Khong tim thay file: " << filePath << endl;
        return;
    }

    streamsize size = file.tellg();
    if (size > 10 * 1024 * 1024) { // Giới hạn 10MB
        cout << "[ERROR] File qua lon (>10MB). Khong gui." << endl;
        return;
    }
    
    // Đọc dữ liệu vào buffer
    file.seekg(0, ios::beg);
    vector<char> buffer(size);
    if (!file.read(buffer.data(), size)) {
        cout << "[ERROR] Loi doc file." << endl;
        return;
    }
    
    // 2. Lấy extension (đuôi file)
    string extension = ".bin";
    size_t dotPos = filePath.find_last_of(".");
    if (dotPos != string::npos) {
        extension = filePath.substr(dotPos);
    }

    // 3. Đóng gói: [TopicID] [Ext] [DataLen] [Data]
    // Lưu ý: Cấu trúc REQ_PUBLISH_BIN = 26: Payload: [topic_id][ext_len][ext][len][data]
    
    PacketBuilder builder;
    builder.addInt(topicId);
    builder.addString(extension);
    builder.addInt((uint32_t)size);
    
    // Ghép dữ liệu binary vào cuối vector payload
    vector<uint8_t> finalPayload = builder.getData();
    finalPayload.insert(finalPayload.end(), buffer.begin(), buffer.end());

    // 4. Gửi
    NetworkUtils::sendPacket(serverSocket, REQ_PUBLISH_BIN, finalPayload.data(), finalPayload.size());
    cout << "[INFO] Da gui file " << filePath << " (" << size << " bytes)." << endl;
}

// --- CÁC HÀM TRA CỨU & HISTORY (SPRINT 3) ---

void ClientHandler::requestHistory(uint32_t topicId) {
    PacketBuilder builder;
    builder.addInt(topicId);
    NetworkUtils::sendPacket(serverSocket, REQ_HISTORY, builder.getData(), builder.getSize());
    cout << "[INFO] Dang lay lich su tin nhan..." << endl;
}

void ClientHandler::requestTopicInfo(uint32_t topicId) {
    PacketBuilder builder;
    builder.addInt(topicId);
    NetworkUtils::sendPacket(serverSocket, REQ_TOPIC_INFO, builder.getData(), builder.getSize());
}

void ClientHandler::requestTopicSubs(uint32_t topicId) {
    PacketBuilder builder;
    builder.addInt(topicId);
    NetworkUtils::sendPacket(serverSocket, REQ_TOPIC_SUBS, builder.getData(), builder.getSize());
}