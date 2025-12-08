#ifndef SERVER_HANDLER_H
#define SERVER_HANDLER_H

#include <cstdint>
#include <string>

class ServerHandler {
public:
    // Constructor nhận socket của client
    ServerHandler(int socket);
    ~ServerHandler();

    // Hàm chạy chính của luồng (Thread loop)
    void run();

private:
    int clientSocket;

    std::string currentUser; // Lưu username sau khi login thành công
    bool isLogged = false;   // Cờ đánh dấu đã login chưa

    // Các hàm xử lý nội bộ
    void handleRegister(const void* payload, uint32_t len);
    void handleLogin(const void* payload, uint32_t len);

    // Các hàm xử lý Topic 
    void handleCreateTopic(const void* payload, uint32_t len);
    void handleDeleteTopic(const void* payload, uint32_t len);
    void handleGetAllTopics();
    void handleGetMyTopics();
};

#endif // SERVER_HANDLER_H