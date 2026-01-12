#ifndef SERVER_HANDLER_H
#define SERVER_HANDLER_H

#include <string>
#include <netinet/in.h>

class ServerHandler {
private:
    int clientSocket;
    bool isLogged;
    std::string currentUser;

    void handleRegister(const void* payload, uint32_t len);
    void handleLogin(const void* payload, uint32_t len);
    void handleCreateTopic(const void* payload, uint32_t len);
    void handleDeleteTopic(const void* payload, uint32_t len);
    void handleGetAllTopics();
    void handleGetMyTopics();
    void handleSubscribe(const void* payload, uint32_t len);
    void handleUnsubscribe(const void* payload, uint32_t len);
    void handlePublishText(const void* payload, uint32_t len);
    void handlePublishBinary(const void* payload, uint32_t len);
    void handleGetHistory(const void* payload, uint32_t len);
    // Các hàm mới thêm:
    void handleGetTopicInfo(uint32_t topicId);
    void handleGetTopicSubs(uint32_t topicId);

public:
    ServerHandler(int socket);
    ~ServerHandler();
    void run();
    static void loadData();
};

#endif