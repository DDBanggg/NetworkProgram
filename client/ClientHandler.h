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

    // --- SPRINT 1: AUTH ---
    bool requestRegister(const std::string& username, const std::string& password);
    bool requestLogin(const std::string& username, const std::string& password);

    // --- SPRINT 2: TOPIC & CHAT ---
    bool requestCreateTopic(std::string topicName);
    bool requestDeleteTopic(std::string topicName);
    void requestGetList(bool isMyTopic);

    // --- SPRINT 3
    bool requestSubscribe(uint32_t topicId);
    bool requestUnsubscribe(uint32_t topicId);
    bool requestPublish(uint32_t topicId, const std::string& message);
};

#endif 