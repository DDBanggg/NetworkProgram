#include "ServerHandler.h"
#include "../common/NetworkUtils.h"
#include "../common/DataUtils.h"
#include "../common/protocol.h"
#include <iostream>
#include <vector>
#include <map>
#include <mutex>
#include <unistd.h> // Header chuẩn cho close() trên Linux

using namespace std;

// --- DATABASE GIẢ LẬP (Lưu trong RAM) ---
// Static để giới hạn phạm vi trong file này, giả lập DB toàn cục
static map<string, string> userDB;
static mutex dbMutex; // Khóa bảo vệ tránh Race Condition

ServerHandler::ServerHandler(int socket) : clientSocket(socket) {}

ServerHandler::~ServerHandler() { 
    close(clientSocket); 
    cout << "Closed connection on socket " << clientSocket << endl;
}

void ServerHandler::run() {
    // Vòng lặp nhận tin nhắn liên tục
    while (true) {
        uint8_t opcode;
        vector<uint8_t> payload;

        // 1. Nhận gói tin (Block chờ)
        bool success = NetworkUtils::recvPacket(clientSocket, opcode, payload);
        if (!success) {
            cout << "Client disconnected or error on socket " << clientSocket << endl;
            break; 
        }

        // 2. Điều hướng xử lý
        switch (opcode) {
            case REQ_REGISTER:
                handleRegister(payload.data(), payload.size());
                break;
            case REQ_LOGIN:
                handleLogin(payload.data(), payload.size());
                break;
            default:
                cout << "Unknown OpCode: " << (int)opcode << endl;
                break;
        }
    }
}

// Xử lý Đăng Ký
void ServerHandler::handleRegister(const void* payloadData, uint32_t len) {
    const uint8_t* ptr = static_cast<const uint8_t*>(payloadData);
    vector<uint8_t> buffer(ptr, ptr + len);
    
    size_t offset = 0;
    string username = DataUtils::unpackString(buffer, offset);
    string password = DataUtils::unpackString(buffer, offset);

    if (username.empty() || password.empty()) {
        uint8_t status = REGISTER_FAIL_INVALID;
        NetworkUtils::sendPacket(clientSocket, RES_REGISTER, &status, 1);
        return;
    }

    uint8_t status;

    // Lock DB
    {
        lock_guard<mutex> lock(dbMutex);
        
        if (userDB.find(username) != userDB.end()) {
            status = REGISTER_FAIL_EXISTS;
            cout << "[REGISTER] Fail (Exist): " << username << endl;
        } else {
            userDB[username] = password;
            status = REGISTER_OK;
            cout << "[REGISTER] Success: " << username << endl;
        }
    }

    // Gửi phản hồi
    NetworkUtils::sendPacket(clientSocket, RES_REGISTER, &status, 1);
}

// Xử lý Đăng Nhập
void ServerHandler::handleLogin(const void* payloadData, uint32_t len) {
    const uint8_t* ptr = static_cast<const uint8_t*>(payloadData);
    vector<uint8_t> buffer(ptr, ptr + len);
    
    size_t offset = 0;
    string username = DataUtils::unpackString(buffer, offset);
    string password = DataUtils::unpackString(buffer, offset);

    uint8_t status;

    // Lock DB
    {
        lock_guard<mutex> lock(dbMutex);

        if (userDB.find(username) == userDB.end()) {
            status = LOGIN_FAIL_NOT_FOUND;
            cout << "[LOGIN] Fail (Not Found): " << username << endl;
        } 
        else if (userDB[username] != password) {
            status = LOGIN_FAIL_WRONG_PASS;
            cout << "[LOGIN] Fail (Wrong Pass): " << username << endl;
        } 
        else {
            status = LOGIN_OK;
            cout << "[LOGIN] Success: " << username << endl;
        }
    }

    // Gửi phản hồi
    NetworkUtils::sendPacket(clientSocket, RES_LOGIN, &status, 1);
}