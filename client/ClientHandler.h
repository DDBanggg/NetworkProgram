
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

    // Nghiệp vụ Sprint 2: Tạo và xóa topic
    bool requestCreateTopic(std::string topicName);
    bool requestDeleteTopic(std::string topicName);
    void requestGetList(bool isMyTopic); // isMyTopic = true (Của tôi), false (Tất cả)
};

#endif // CLIENT_HANDLER_H