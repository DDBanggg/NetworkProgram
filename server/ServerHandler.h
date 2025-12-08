#ifndef SERVER_HANDLER_H
#define SERVER_HANDLER_H

#include <cstdint>

class ServerHandler {
public:
    // Constructor nhận socket của client
    ServerHandler(int socket);
    ~ServerHandler();

    // Hàm chạy chính của luồng (Thread loop)
    void run();

private:
    int clientSocket;

    // Các hàm xử lý nội bộ
    void handleRegister(const void* payload, uint32_t len);
    void handleLogin(const void* payload, uint32_t len);
};

#endif // SERVER_HANDLER_H