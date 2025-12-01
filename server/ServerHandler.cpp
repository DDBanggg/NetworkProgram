#include "ServerHandler.h"
#include "../common/NetworkUtils.h"
#include "../common/DataUtils.h"
#include "../common/protocol.h"
#include <iostream>

// Global/Static giả lập Database (Core tạo sẵn cho Server dùng)
#include <map>
#include <mutex>
static std::map<std::string, std::string> userDB;
static std::mutex dbMutex;

ServerHandler::ServerHandler(int socket) : clientSocket(socket) {}
ServerHandler::~ServerHandler() { close(clientSocket); }

void ServerHandler::run() {
    // TODO: Copy vòng lặp while từ tài liệu hướng dẫn vào đây
    // Dùng switch(opcode) để gọi handleRegister/handleLogin
}

void ServerHandler::handleRegister(const void* payloadData, uint32_t len) {
    // HƯỚNG DẪN CHO BẠN LÀM SERVER:
    // 1. Chuyển payloadData sang vector để dễ xử lý:
    //    std::vector<uint8_t> buffer((uint8_t*)payloadData, (uint8_t*)payloadData + len);
    // 2. Dùng DataUtils::unpackString để lấy user và pass.
    // 3. Lock mutex -> Check map -> Unlock.
    // 4. Gửi phản hồi dùng NetworkUtils::sendPacket.
}

void ServerHandler::handleLogin(const void* payloadData, uint32_t len) {
    // TODO: Tương tự handleRegister
}