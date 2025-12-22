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
    PacketBuilder builder;
    builder.addString(username);
    builder.addString(password);

    if (!NetworkUtils::sendPacket(serverSocket, REQ_REGISTER, builder.getData(), builder.getSize())) return false;

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

    if (!NetworkUtils::sendPacket(serverSocket, REQ_LOGIN, builder.getData(), builder.getSize())) return false;

    uint8_t op; vector<uint8_t> res;
    if (!NetworkUtils::recvPacket(serverSocket, op, res)) return false;

    if (op == RES_LOGIN && !res.empty()) {
        if (res[0] == LOGIN_OK) {
            cout << ">>> DANG NHAP THANH CONG! <<<" << endl;
            return true;
        } else if (res[0] == LOGIN_FAIL_NOT_FOUND) cout << ">>> THAT BAI: Tai khoan khong ton tai." << endl;
        else if (res[0] == LOGIN_FAIL_WRONG_PASS) cout << ">>> THAT BAI: Sai mat khau." << endl;
    }
    return false;
}

// --- NHÓM CHỨC NĂNG (Khi đã Login -> Listener đang chạy -> CHỈ GỬI, KHÔNG NHẬN) ---

bool ClientHandler::requestCreateTopic(string topicName) {
    PacketBuilder builder;
    builder.addString(topicName);
    return NetworkUtils::sendPacket(serverSocket, REQ_CREATE_TOPIC, builder.getData(), builder.getSize());
}

bool ClientHandler::requestDeleteTopic(string topicName) {
    PacketBuilder builder;
    builder.addString(topicName);
    return NetworkUtils::sendPacket(serverSocket, REQ_DELETE_TOPIC, builder.getData(), builder.getSize());
}

void ClientHandler::requestGetList(bool isMyTopic) {
    uint8_t reqOp = isMyTopic ? REQ_GET_MY_TOPICS : REQ_GET_ALL_TOPICS;
    NetworkUtils::sendPacket(serverSocket, reqOp, nullptr, 0);
}

bool ClientHandler::requestSubscribe(uint32_t topicId) {
    PacketBuilder builder;
    builder.addInt(topicId);
    return NetworkUtils::sendPacket(serverSocket, REQ_SUBSCRIBE, builder.getData(), builder.getSize());
}

bool ClientHandler::requestUnsubscribe(uint32_t topicId) {
    PacketBuilder builder;
    builder.addInt(topicId);
    return NetworkUtils::sendPacket(serverSocket, REQ_UNSUBSCRIBE, builder.getData(), builder.getSize());
}

bool ClientHandler::requestPublish(uint32_t topicId, const string& message) {
    PacketBuilder builder;
    builder.addInt(topicId);
    builder.addString(message);
    return NetworkUtils::sendPacket(serverSocket, REQ_PUBLISH_TEXT, builder.getData(), builder.getSize());
}