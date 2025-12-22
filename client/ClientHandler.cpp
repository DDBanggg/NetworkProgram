#include "ClientHandler.h"
#include "../common/NetworkUtils.h"
#include "../common/DataUtils.h" 
#include "../common/protocol.h"
#include <iostream>

using namespace std;

ClientHandler::ClientHandler(int socket) : serverSocket(socket) {}
ClientHandler::~ClientHandler() {}

// ==========================================================
// PHẦN 1: AUTHENTICATION (Đăng ký / Đăng nhập)
// ==========================================================

bool ClientHandler::requestRegister(const string& username, const string& password) {
    // [FIX] Dùng PacketBuilder thay vì DataUtils::packString cũ
    PacketBuilder builder;
    builder.addString(username);
    builder.addString(password);

    if (!NetworkUtils::sendPacket(serverSocket, REQ_REGISTER, builder.getData(), builder.getSize())) {
        return false;
    }

    uint8_t op; vector<uint8_t> res;
    if (!NetworkUtils::recvPacket(serverSocket, op, res)) return false;

    if (op == RES_REGISTER && !res.empty()) {
        if (res[0] == REGISTER_OK) {
            cout << ">>> DANG KY THANH CONG! <<<" << endl;
            return true;
        } else if (res[0] == REGISTER_FAIL_EXISTS) {
            cout << ">>> THAT BAI: Tai khoan da ton tai." << endl;
        }
    }
    return false;
}

bool ClientHandler::requestLogin(const string& username, const string& password) {
    PacketBuilder builder;
    builder.addString(username);
    builder.addString(password);

    if (!NetworkUtils::sendPacket(serverSocket, REQ_LOGIN, builder.getData(), builder.getSize())) {
        return false;
    }

    uint8_t op; vector<uint8_t> res;
    if (!NetworkUtils::recvPacket(serverSocket, op, res)) return false;

    if (op == RES_LOGIN && !res.empty()) {
        if (res[0] == LOGIN_OK) {
            cout << ">>> DANG NHAP THANH CONG! <<<" << endl;
            return true;
        } else if (res[0] == LOGIN_FAIL_NOT_FOUND) {
            cout << ">>> THAT BAI: Tai khoan khong ton tai." << endl;
        } else if (res[0] == LOGIN_FAIL_WRONG_PASS) {
            cout << ">>> THAT BAI: Sai mat khau." << endl;
        }
    }
    return false;
}

// ==========================================================
// PHẦN 2: QUẢN LÝ TOPIC (Tạo / Xóa / Xem)
// ==========================================================

bool ClientHandler::requestCreateTopic(string topicName) {
    PacketBuilder builder;
    builder.addString(topicName);

    if (!NetworkUtils::sendPacket(serverSocket, REQ_CREATE_TOPIC, builder.getData(), builder.getSize())) {
        return false;
    }

    uint8_t op; vector<uint8_t> res;
    if (!NetworkUtils::recvPacket(serverSocket, op, res)) return false;

    if (op == RES_CREATE_TOPIC && !res.empty() && res[0] == TOPIC_CREATE_OK) {
        return true;
    }
    cout << ">> Loi tao topic (Co the da ton tai)!" << endl;
    return false;
}

bool ClientHandler::requestDeleteTopic(string topicName) {
    PacketBuilder builder;
    builder.addString(topicName);

    if (!NetworkUtils::sendPacket(serverSocket, REQ_DELETE_TOPIC, builder.getData(), builder.getSize())) {
        return false;
    }

    uint8_t op; vector<uint8_t> res;
    if (!NetworkUtils::recvPacket(serverSocket, op, res)) return false;

    if (op == RES_DELETE_TOPIC && !res.empty()) {
        if (res[0] == TOPIC_DELETE_OK) return true;
        if (res[0] == TOPIC_FAIL_DENIED) cout << ">> Ban khong phai chu topic!" << endl;
        else cout << ">> Xoa that bai!" << endl;
    }
    return false;
}

void ClientHandler::requestGetList(bool isMyTopic) {
    uint8_t reqOp = isMyTopic ? REQ_GET_MY_TOPICS : REQ_GET_ALL_TOPICS;
    uint8_t resOp = isMyTopic ? RES_GET_MY_TOPICS : RES_GET_ALL_TOPICS;

    if (!NetworkUtils::sendPacket(serverSocket, reqOp, nullptr, 0)) return;

    uint8_t op; vector<uint8_t> res;
    if (!NetworkUtils::recvPacket(serverSocket, op, res)) return;

    if (op == resOp) {
        PacketReader reader(res);
        uint32_t count = reader.readInt();
        
        cout << "\n--- DANH SACH (" << count << " Topics) ---" << endl;
        for (uint32_t i = 0; i < count; ++i) {
            string name = reader.readString();
            string creator = reader.readString();
            cout << "#" << (i + 1) << ". [" << name << "] - Creator: " << creator << endl;
        }
        cout << "-----------------------------------" << endl;
    }
}

// ==========================================================
// PHẦN 3: SUBSCRIBE & CHAT (Mới bổ sung)
// ==========================================================

bool ClientHandler::requestSubscribe(uint32_t topicId) {
    PacketBuilder builder;
    builder.addInt(topicId);

    if (!NetworkUtils::sendPacket(serverSocket, REQ_SUBSCRIBE, builder.getData(), builder.getSize())) return false;

    uint8_t op; vector<uint8_t> res;
    if (!NetworkUtils::recvPacket(serverSocket, op, res)) return false;
    return (op == RES_SUBSCRIBE && !res.empty() && res[0] == SUB_OK);
}

bool ClientHandler::requestUnsubscribe(uint32_t topicId) {
    PacketBuilder builder;
    builder.addInt(topicId);

    if (!NetworkUtils::sendPacket(serverSocket, REQ_UNSUBSCRIBE, builder.getData(), builder.getSize())) return false;

    uint8_t op; vector<uint8_t> res;
    if (!NetworkUtils::recvPacket(serverSocket, op, res)) return false;
    return (op == RES_UNSUBSCRIBE && !res.empty() && res[0] == UNSUB_OK);
}

bool ClientHandler::requestPublish(uint32_t topicId, const string& message) {
    PacketBuilder builder;
    builder.addInt(topicId);
    builder.addString(message);

    if (!NetworkUtils::sendPacket(serverSocket, REQ_PUBLISH_TEXT, builder.getData(), builder.getSize())) return false;

    uint8_t op; vector<uint8_t> res;
    if (!NetworkUtils::recvPacket(serverSocket, op, res)) return false;
    return (op == RES_PUBLISH && !res.empty() && res[0] == STATUS_SUCCESS);
}