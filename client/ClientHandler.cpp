// client/ClientHandler.cpp
#include "ClientHandler.h"
#include "../common/NetworkUtils.h"
#include "../common/DataUtils.h"
#include "../common/protocol.h"
#include <iostream>

using namespace std;

ClientHandler::ClientHandler(int socket) : serverSocket(socket) {}

ClientHandler::~ClientHandler() {
    // Không đóng socket ở đây vì socket do ClientApp quản lý vòng đời
}

bool ClientHandler::requestRegister(const string& username, const string& password) {
    // 1. Đóng gói Payload: [user_len][user][pass_len][pass]
    // DataUtils::packString đã tự động thêm 4 bytes độ dài (Big Endian) ở đầu chuỗi
    vector<uint8_t> payload = DataUtils::packString(username);
    vector<uint8_t> passBytes = DataUtils::packString(password);
    
    // Nối password vào sau username
    payload.insert(payload.end(), passBytes.begin(), passBytes.end());

    // 2. Gửi Request (REQ_REGISTER)
    if (!NetworkUtils::sendPacket(serverSocket, REQ_REGISTER, payload.data(), payload.size())) {
        cerr << "Error: Failed to send Register request." << endl;
        return false;
    }

    // 3. Chờ phản hồi NGAY LẬP TỨC (Block chờ) [cite: 152]
    uint8_t resOpcode;
    vector<uint8_t> resPayload;
    if (!NetworkUtils::recvPacket(serverSocket, resOpcode, resPayload)) {
        cerr << "Error: Connection lost while waiting for response." << endl;
        return false;
    }

    // 4. Xử lý kết quả
    if (resOpcode == RES_REGISTER) {
        if (resPayload.empty()) return false;
        uint8_t status = resPayload[0];

        if (status == REGISTER_OK) {
            cout << ">>> DANG KY THANH CONG! <<<" << endl;
            return true;
        } else if (status == REGISTER_FAIL_EXISTS) {
            cout << ">>> THAT BAI: Tai khoan da ton tai." << endl;
            return false;
        } else {
            cout << ">>> THAT BAI: Loi khong xac dinh." << endl;
            return false;
        }
    }

    return false;
}

bool ClientHandler::requestLogin(const string& username, const string& password) {
    // 1. Đóng gói Payload (Giống hệt Register) [cite: 160]
    vector<uint8_t> payload = DataUtils::packString(username);
    vector<uint8_t> passBytes = DataUtils::packString(password);
    payload.insert(payload.end(), passBytes.begin(), passBytes.end());

    // 2. Gửi Request (REQ_LOGIN)
    if (!NetworkUtils::sendPacket(serverSocket, REQ_LOGIN, payload.data(), payload.size())) {
        cerr << "Error: Failed to send Login request." << endl;
        return false;
    }

    // 3. Chờ phản hồi
    uint8_t resOpcode;
    vector<uint8_t> resPayload;
    if (!NetworkUtils::recvPacket(serverSocket, resOpcode, resPayload)) {
        cerr << "Error: Connection lost." << endl;
        return false;
    }

    // 4. Xử lý kết quả
    if (resOpcode == RES_LOGIN) {
        if (resPayload.empty()) return false;
        uint8_t status = resPayload[0];

        if (status == LOGIN_OK) {
            cout << ">>> DANG NHAP THANH CONG! <<<" << endl;
            return true;
        } else if (status == LOGIN_FAIL_NOT_FOUND) {
            cout << ">>> THAT BAI: Tai khoan khong ton tai." << endl;
            return false;
        } else if (status == LOGIN_FAIL_WRONG_PASS) {
            cout << ">>> THAT BAI: Sai mat khau." << endl;
            return false;
        }
    }

    return false;
}