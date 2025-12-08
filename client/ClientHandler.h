// client/ClientHandler.h
#ifndef CLIENT_HANDLER_H
#define CLIENT_HANDLER_H

#include <string>
#include <vector>
#include <cstdint>

class ClientHandler {
private:
    int serverSocket;

public:
    explicit ClientHandler(int socket);
    ~ClientHandler();

    // Nghiệp vụ Sprint 1: Đăng ký và Đăng nhập
    // Trả về true nếu thành công, false nếu thất bại (kèm in lỗi)
    bool requestRegister(const std::string& username, const std::string& password);
    bool requestLogin(const std::string& username, const std::string& password);

    // Sprint 2: Sau này sẽ thêm các hàm như joinTopic, sendMessage ở đây
};

#endif // CLIENT_HANDLER_H